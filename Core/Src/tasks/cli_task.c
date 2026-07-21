/**
* @file cli_task.c
* @author Cole Bardin (cab572@drexel.edu)
* @brief
* @version 0.1
* @date 2023-10-24
*
* @copyright Copyright (c) 2023
*
*/

#include "tasks/cli_task.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/**
* @brief Actual CLI task function
*
* @param arg App_data struct pointer converted to void pointer
*/
void cli_task_fn(void *arg);
int cli_handle_cmd(int argc, char *argv[]);
void cmd_not_found(int argc, char *argv[]);

int help(int argc, char *argv[]);
int id(int argc, char *argv[]);
int get_throttle(int argc, char *argv[]);
int get_brakelight(int argc, char *argv[]);
int get_brake(int argc, char *argv[]);
int get_time(int argc, char *argv[]);
int set_time(int argc, char *argv[]);
int get_faults(int argc, char *argv[]);
int get_status(int argc, char *argv[]);
int get_ams_status(int argc, char *argv[]);
int get_can_status(int argc, char *argv[]);
int get_bspd_status(int argc, char *argv[]);
int ssa(int argc, char *argv[]);
int sd(int argc, char *argv[]);

char outline[CLI_LINESZ];
app_data_t *data;
cli_t *cli;
command_t cmds[] =
{
	{"help", &help, "print help menu"},
	{"id", &id, "identifies system"},
	{"ver", &id, "firmware version, build profile, and output lock"},
	{"status", &get_status, "critical ECU state and output summary"},
	{"ams", &get_ams_status, "AMS compact-frame safety status"},
	{"can", &get_can_status, "CAN error and CM200 counter status"},
	{"bspd", &get_bspd_status, "BSPD raw-OK and decoded fault status"},
	{"throttle", &get_throttle, "get the throttle percentage"},
	{"brakelight", &get_brakelight, "get the brake light status"},
	{"brake", &get_brake, "get the brake percentage"},
	{"gtime", &get_time, "get the RTC"},
	{"stime", &set_time, "set the RTC. format: '1/2/24-17:38:50' for Jan. 2, 2024 at 5:38:50PM"},
	{"fault", &get_faults, "gets the faults of the system"},
	{"ssa", &ssa, "set the SSA light duty cycle"},
	{"sd", &sd, "print the shutdown circuit states"}
};

TaskHandle_t cli_task_start(app_data_t *data)
{
   TaskHandle_t handle = NULL;

   if(data == NULL)
   {
       return NULL;
   }

   xTaskCreate(cli_task_fn, "CLI task", 1024, (void *)data, CLI_PRIO, &handle);
   return handle;
}

void cli_task_fn(void *arg)
{
    data = (app_data_t *)arg;
    if(data == NULL)
    {
        vTaskDelete(NULL);
        return;
    }

    cli = &data->board.cli;
    uint32_t entry;
    char buf[CLI_LINESZ] = {0};
    char *tokens[MAXTOKS];
    int n;
    int ret = 0;
    size_t len;
	
	cli_printline(cli, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
	cli_printline(cli, "Type 'help' for list of commands");

	for(;;)
	{
		entry = osKernelGetTickCount();
		if(cli->msg_pending == true)
		{
			taskENTER_CRITICAL();
			len = strnlen(cli->line, CLI_LINESZ - 1);
			memcpy(buf, cli->line, len + 1);
			memset(cli->line, 0, len + 1);
			cli->msg_pending = false;
			taskEXIT_CRITICAL();
			buf[len] = '\0';
			n = tokenize(buf, tokens, MAXTOKS, " \t");
			ret = cli_handle_cmd(n, tokens);
			data->cli_fault = ret;
			cli->msg_proc++;
		}
		osDelayUntil(entry + (1000 / CLI_FREQ));
	}
}

int cli_handle_cmd(int argc, char *argv[])
{
	int i;
	int ret = 0;
	bool cmd_found = false;
	int num_cmds = sizeof(cmds) / sizeof(command_t);

	if((argc <= 0) || (argv == NULL) || (argv[0] == NULL))
	{
		return 0;
	}

	for(i = 0; i < num_cmds; i++)
	{
		if(!strncmp(cmds[i].name, argv[0], CLI_LINESZ))
		{
			ret = cmds[i].func(argc, argv);
			cli->msg_valid++;
			cmd_found = true;
			break;
		}
	}
	if(!cmd_found) cmd_not_found(argc, argv);
	cli->ret = ret;
	return ret;
}

void cmd_not_found(int argc, char *argv[])
{
	snprintf(outline, CLI_LINESZ, "Command not found: \'%s\'", argv[0]);
	cli_printline(cli, outline);
	cli_printline(cli, "Type 'help' for list of commands");
}

int help(int argc, char *argv[])
{
	int num_cmds;
	int i;

	cli_printline(cli, "---------- Help Menu ----------");
	num_cmds = sizeof(cmds) / sizeof(command_t);
	for(i = 0; i < num_cmds; i++)
	{
		snprintf(outline, CLI_LINESZ, "%s - %s", cmds[i].name, cmds[i].desc);
		cli_printline(cli, outline);
	}
	return 0;
}

int id(int argc, char *argv[])
{
    snprintf(outline, CLI_LINESZ, "DER ECU FW V%d.%d.%d", VER_MAJOR, VER_MINOR, VER_BUG);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ, "profile:%s outputs_inhibited:%u bspd_3v3_validated:%u",
	         (ECU_BUILD_PROFILE == ECU_BUILD_PROFILE_VEHICLE) ? "vehicle" : "bench",
	         (unsigned)ECU_OUTPUTS_INHIBITED,
	         (unsigned)ECU_BSPD_INTERFACE_3V3_VALIDATED);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ, "reset_cause:0x%08lX watchdog:%u",
	         (unsigned long)data->reset_cause, (unsigned)ECU_ENABLE_IWDG);
	cli_printline(cli, outline);
	return 0;
}

int get_status(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	snprintf(outline, CLI_LINESZ,
	         "RTD:%u torque:%d brake:%d hard:%u soft:%u startup:%u hb:%u",
	         (unsigned)data->rtd_mode, data->throttle, data->brake,
	         (unsigned)data->hard_fault, (unsigned)data->soft_fault,
	         (unsigned)data->startup_fault, (unsigned)data->task_heartbeat_fault);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ,
	         "OUT fw_ok:%u mtr_en:%u mtr_on:%u inhibited:%u",
	         (unsigned)data->fw_state, (unsigned)data->cascadia_en,
	         (unsigned)data->cascadia_on, (unsigned)ECU_OUTPUTS_INHIBITED);
	cli_printline(cli, outline);
	return 0;
}

int get_ams_status(int argc, char *argv[])
{
	ams_t snap;
	(void)argc;
	(void)argv;
	taskENTER_CRITICAL();
	memcpy(&snap, &data->board.ams, sizeof(snap));
	taskEXIT_CRITICAL();

	snprintf(outline, CLI_LINESZ,
	         "AMS allow:%u stale:%u S/E/T:%u/%u/%u seq:%u seqfault:%u",
	         (unsigned)ams_allows_torque(&snap), (unsigned)snap.stale,
	         (unsigned)snap.compact_status_valid,
	         (unsigned)snap.compact_electrical_valid,
	         (unsigned)snap.compact_thermal_valid,
	         (unsigned)snap.compact_sequence,
	         (unsigned)snap.compact_sequence_fault);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ,
	         "AMS bms_ok:%u inhibit:%u flags:%02X/%02X thermal:%02X",
	         (unsigned)snap.bms_ok, (unsigned)snap.bms_inhibited,
	         (unsigned)snap.compact_status_flags,
	         (unsigned)snap.compact_fault_flags,
	         (unsigned)snap.thermal_flags);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ,
	         "AMS V:%u.%uV I:%d.%uA cell:%u..%umV temp:%d..%d dC",
	         (unsigned)(snap.pack_voltage_0p1v / 10u),
	         (unsigned)(snap.pack_voltage_0p1v % 10u),
	         (int)(snap.pack_current_0p1a / 10),
	         (unsigned)((snap.pack_current_0p1a < 0 ? -snap.pack_current_0p1a : snap.pack_current_0p1a) % 10),
	         (unsigned)snap.min_cell_mv, (unsigned)snap.max_cell_mv,
	         (int)snap.min_temp_0p1c, (int)snap.max_temp_0p1c);
	cli_printline(cli, outline);
	return 0;
}

int get_can_status(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	snprintf(outline, CLI_LINESZ,
	         "CAN fault:%u rx:%u tx:%u hw:%u err:0x%08lX",
	         (unsigned)data->canbus_fault, (unsigned)data->canbus_rx_fault,
	         (unsigned)data->canbus_tx_fault, (unsigned)data->canbus_hw_fault,
	         (unsigned long)data->can_error_code);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ,
	         "CAN dropped:%lu overruns:%lu recoveries:%lu cm_counter:%u",
	         (unsigned long)data->board.canbus.tx_dropped_count,
	         (unsigned long)data->can_rx_overrun_count,
	         (unsigned long)data->can_recovery_count,
	         (unsigned)data->cm200_rolling_counter);
	cli_printline(cli, outline);
	return 0;
}

int get_bspd_status(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	snprintf(outline, CLI_LINESZ,
	         "BSPD net:BSPD_OK raw:%u decoded_fault:%u interface_3v3:%u",
	         (unsigned)data->bspd_ok_raw, (unsigned)data->bspd_fail,
	         (unsigned)ECU_BSPD_INTERFACE_3V3_VALIDATED);
	cli_printline(cli, outline);
	return 0;
}

int get_throttle(int argc, char *argv[])
{
	snprintf(outline, CLI_LINESZ, "throttle: %3d%%", data->throttle);
	cli_printline(cli, outline);
	return 0;
}

int get_brakelight(int argc, char *argv[])
{
	snprintf(outline, CLI_LINESZ, "brakelight: %s", data->brakelight ? "ON" : "OFF");
	cli_printline(cli, outline);
	return 0;
}

int get_brake(int argc, char *argv[])
{
	snprintf(outline, CLI_LINESZ, "brake: %3d%%", data->brake);
	cli_printline(cli, outline);
	return 0;
}

int get_time(int argc, char *argv[])
{
	read_time();
	snprintf(outline, CLI_LINESZ, "RTC: %02d/%02d/%d-%02d:%02d:%02d",
			data->datetime.month,
			data->datetime.day,
			data->datetime.year,
			data->datetime.hour,
			data->datetime.minute,
			data->datetime.second);
	cli_printline(cli, outline);
	return 0;
}

int set_time(int argc, char *argv[])
{
	int month, day, year, hour, minute, second;

	if(argc != 2)
	{
		cli_printline(cli, "ERROR: stime takes 1 argument");
		cli_printline(cli, "usage: 'stime 1/2/24-17:38:50' for Jan. 2, 2024 at 5:38:50PM");
		return 1;
	}
	int ret = sscanf(argv[1], "%d/%d/%d-%d:%d:%d",
			&month,
			&day,
			&year,
			&hour,
			&minute,
			&second);

	if(ret != 6){
		cli_printline(cli, "ERROR: set time format not readable");
		cli_printline(cli, "usage: 'stime 1/2/24-17:38:50' for Jan. 2, 2024 at 5:38:50PM");
		return 1;
	}

	data->datetime.month = month;
	data->datetime.day = day;
	data->datetime.year = year;
	data->datetime.hour = hour;
	data->datetime.minute = minute;
	data->datetime.second = second;
	write_time();

	snprintf(outline, CLI_LINESZ, "Set RTC: %02d/%02d/%d-%02d:%02d:%02d",
			data->datetime.month,
			data->datetime.day,
			data->datetime.year,
			data->datetime.hour,
			data->datetime.minute,
			data->datetime.second
			);
	cli_printline(cli, outline);

	return 0;
}

int get_faults(int argc, char *argv[])
{
	cli_printline(cli, "System faults:");
	snprintf(outline, CLI_LINESZ, "hard:   %d", data->hard_fault);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ, "  apps:    %d", data->apps_fault);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ, "  bse:     %d", data->bse_fault);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ, "  coolant: %d", data->coolant_fault);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ, "soft:   %d", data->soft_fault);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ, "  bppc:    %d", data->bppc_fault);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ, "  acc:     %d", data->acc_fault);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ, "  cli:     %d", data->cli_fault);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ, "  canbus:  %d", data->canbus_fault);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ, "    rx:    %d", data->canbus_rx_fault);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ, "    tx:    %d", data->canbus_tx_fault);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ, "  dash:    %d", data->dashboard_fault);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ, "  mq:      %d", data->mq_fault);
	cli_printline(cli, outline);
	return 0;
}

int ssa(int argc, char *argv[])
{
	int ret = 0;
	if(argc == 1)
	{
		snprintf(outline, CLI_LINESZ, "%d%%", (int)(TIM3->CCR4 * 100 / 65535));
		cli_printline(cli, outline);
	}
	else if(argc == 2)
	{
		int duty = atoi(argv[1]);
		if(duty >= 0 && duty <= 100)
		{
			snprintf(outline, CLI_LINESZ, "setting ssa to %d%%", duty);
			cli_printline(cli, outline);
			set_ssa(duty);
			ret = 0;
		}
		else
		{
			cli_printline(cli, "ERROR: ssa duty must be 0<= Duty <= 100");
			return 1;
		}
	}
	else
	{
		cli_printline(cli, "ERROR: too many arguments to ssa");
		return 1;
	}
	return ret;
}

int sd(int argc, char *argv[])
{
	int ret = 0;
	snprintf(outline, CLI_LINESZ, "bms fail:  %d", data->bms_fail);
	ret |= cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ, "imd fail:  %d", data->imd_fail);
	ret |= cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ, "bspd fail: %d", data->bspd_fail);
	ret |= cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ, "bspd raw OK: %d", data->bspd_ok_raw);
	ret |= cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ, "fw fail:   %d", !data->fw_state);
	ret |= cli_printline(cli, outline);
	return ret;
}
