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

static StaticTask_t cli_task_tcb;
static StackType_t cli_task_stack[ECU_STACK_CLI_WORDS];
static TaskHandle_t cli_task_handle = NULL;
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
int get_cm200_status(int argc, char *argv[]);
int get_task_status(int argc, char *argv[]);
int get_bspd_status(int argc, char *argv[]);
int get_power_status(int argc, char *argv[]);
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
	{"cm200", &get_cm200_status, "CM200 broadcasts, freshness, faults, and command integrity"},
	{"tasks", &get_task_status, "task stack high-water marks in words"},
	{"bspd", &get_bspd_status, "BSPD raw-OK and decoded fault status"},
	{"power", &get_power_status, "torque clamp, current model, and residual-monitor status"},
	{"throttle", &get_throttle, "get the throttle percentage"},
	{"brakelight", &get_brakelight, "get the brake light status"},
	{"brake", &get_brake, "get the brake percentage"},
	{"gtime", &get_time, "get the RTC"},
	{"stime", &set_time, "set the RTC. format: '1/2/24-17:38:50' for Jan. 2, 2024 at 5:38:50PM"},
	{"fault", &get_faults, "gets the faults of the system"},
	{"ssa", &ssa, "set the SSA light duty cycle"},
	{"sd", &sd, "print the shutdown circuit states"}
};

TaskHandle_t cli_task_start(app_data_t *app_data)
{
    if(app_data == NULL)
    {
        return NULL;
    }

    if(cli_task_handle == NULL)
    {
        cli_task_handle = xTaskCreateStatic(cli_task_fn,
            "CLI task", ECU_STACK_CLI_WORDS, (void *)app_data, CLI_PRIO,
            cli_task_stack, &cli_task_tcb);
    }
    return cli_task_handle;
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
	snprintf(outline, CLI_LINESZ, "profile:%s inhibited:%u bspd_3v3:%u cm200_contract:%u",
	         (ECU_BUILD_PROFILE == ECU_BUILD_PROFILE_VEHICLE) ? "vehicle" : "bench",
	         (unsigned)ECU_OUTPUTS_INHIBITED,
	         (unsigned)ECU_BSPD_INTERFACE_3V3_VALIDATED,
	         (unsigned)ECU_CM200_CAN_CONTRACT_VALIDATED);
	cli_printline(cli, outline);
    snprintf(outline, CLI_LINESZ, "ams_clamp_impl:%u ams_clamp_valid:%u",
             (unsigned)ECU_AMS_POWER_CLAMP_IMPLEMENTED,
             (unsigned)ECU_AMS_POWER_CLAMP_VALIDATED);
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
	         "RTD:%u throttle:%d brake:%d hard:%u soft:%u startup:%u hb:%u cm:%u",
	         (unsigned)data->rtd_mode, data->throttle, data->brake,
	         (unsigned)data->hard_fault, (unsigned)data->soft_fault,
	         (unsigned)data->startup_fault, (unsigned)data->task_heartbeat_fault,
	         (unsigned)data->cm200_fault);
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
	         "CAN fault:%u rx:%u tx:%u hw:%u filters:%u err:0x%08lX",
	         (unsigned)data->canbus_fault, (unsigned)data->canbus_rx_fault,
	         (unsigned)data->canbus_tx_fault, (unsigned)data->canbus_hw_fault,
	         (unsigned)data->board.canbus.filters_configured,
	         (unsigned long)data->can_error_code);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ,
	         "CAN tx_drop:%lu replaced:%lu overruns:%lu recoveries:%lu cm_ctr:%u",
	         (unsigned long)data->board.canbus.tx_dropped_count,
	         (unsigned long)data->board.canbus.tx_replaced_count,
	         (unsigned long)data->can_rx_overrun_count,
	         (unsigned long)data->can_recovery_count,
	         (unsigned)data->cm200_rolling_counter);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ,
	         "CAN rx_ok:%lu ignored:%lu malformed:%lu remote:%lu",
	         (unsigned long)data->board.canbus.rx_accepted_count,
	         (unsigned long)data->board.canbus.rx_ignored_count,
	         (unsigned long)data->board.canbus.rx_malformed_count,
	         (unsigned long)data->board.canbus.rx_remote_count);
	cli_printline(cli, outline);
	return 0;
}

int get_cm200_status(int argc, char *argv[])
{
	cm200_t snap;
	ams_t ams_snap;
	(void)argc;
	(void)argv;

	taskENTER_CRITICAL();
	memcpy(&snap, &data->board.cm200, sizeof(snap));
	memcpy(&ams_snap, &data->board.ams, sizeof(ams_snap));
	taskEXIT_CRITICAL();

	snprintf(outline, CLI_LINESZ,
	         "CM feedback:%u torque_ready:%u fault:%u seen:%u latch S/R:%u/%u",
	         (unsigned)cm200_feedback_healthy(&snap),
	         (unsigned)cm200_allows_torque(&snap),
	         (unsigned)data->cm200_fault,
	         (unsigned)data->cm200_feedback_seen,
	         (unsigned)data->cm200_startup_timeout,
	         (unsigned)data->cm200_runtime_fault_latched);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ,
	         "CM fresh A5/A7/AA/AB/AC/B1:%u/%u/%u/%u/%u/%u",
	         (unsigned)!snap.frame[CM200_FRAME_MOTOR_POSITION].stale,
	         (unsigned)!snap.frame[CM200_FRAME_VOLTAGE].stale,
	         (unsigned)!snap.frame[CM200_FRAME_INTERNAL_STATES].stale,
	         (unsigned)!snap.frame[CM200_FRAME_FAULTS].stale,
	         (unsigned)!snap.frame[CM200_FRAME_TORQUE_TIMER].stale,
	         (unsigned)!snap.frame[CM200_FRAME_TORQUE_CAPABILITY].stale);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ,
	         "CM bus:%ddV AMS:%udV delta:%ddV check:%u mismatch:%u",
	         (int)snap.dc_bus_voltage_0p1v,
	         (unsigned)ams_snap.pack_voltage_0p1v,
	         (int)data->ams_cm200_voltage_delta_0p1v,
	         (unsigned)data->ams_cm200_voltage_crosscheck_valid,
	         (unsigned)data->ams_cm200_voltage_mismatch);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ,
	         "CM speed:%drpm torque cmd/fb/cap:%d/%d/%d dNm",
	         (int)snap.motor_speed_rpm,
	         (int)snap.commanded_torque_0p1nm,
	         (int)snap.torque_feedback_0p1nm,
	         (int)snap.motor_torque_available_0p1nm);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ,
	         "CM VSM:%u INV:%u CAN:%u torque_mode:%u lockout:%u enabled:%u",
	         (unsigned)snap.vsm_state,
	         (unsigned)snap.inverter_state,
	         (unsigned)snap.command_mode_can,
	         (unsigned)snap.torque_mode,
	         (unsigned)snap.inverter_enable_lockout,
	         (unsigned)snap.inverter_enabled);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ,
	         "CM states mode:%02X enable:%02X limits:%02X BMS_active:%u",
	         (unsigned)snap.mode_states,
	         (unsigned)snap.enable_states,
	         (unsigned)snap.limit_states,
	         (unsigned)snap.bms_active);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ,
	         "CM counter ecu/expected:%u/%u sync:%u mism:%u echo:%u mism:%u",
	         (unsigned)snap.last_command_counter,
	         (unsigned)snap.inverter_expected_counter,
	         (unsigned)snap.counter_synced,
	         (unsigned)snap.counter_mismatch_count,
	         (unsigned)snap.torque_echo_synced,
	         (unsigned)snap.torque_echo_mismatch_count);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ,
	         "CM faults POST:0x%08lX RUN:0x%08lX timer:%lu timer_fault:%u",
	         (unsigned long)snap.post_faults,
	         (unsigned long)snap.run_faults,
	         (unsigned long)snap.power_on_timer,
	         (unsigned)snap.timer_reset_fault);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ,
	         "CM temp dC modules:%d/%d/%d coolant:%d motor:%d inv_hot:%d",
	         (int)snap.module_a_temp_0p1c,
	         (int)snap.module_b_temp_0p1c,
	         (int)snap.module_c_temp_0p1c,
	         (int)snap.coolant_temp_0p1c,
	         (int)snap.motor_temp_0p1c,
	         (int)snap.inverter_hotspot_temp_0p1c);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ,
	         "CM firmware project:%04X sw:%04X date:%04X/%u valid:%u",
	         (unsigned)snap.eeprom_project_code,
	         (unsigned)snap.software_version,
	         (unsigned)snap.date_mmdd,
	         (unsigned)snap.date_year,
	         (unsigned)snap.frame[CM200_FRAME_FIRMWARE].valid);
	cli_printline(cli, outline);
	return 0;
}

static unsigned long task_stack_words(TaskHandle_t task)
{
    return (task == NULL) ? 0u : (unsigned long)uxTaskGetStackHighWaterMark(task);
}

int get_task_status(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	snprintf(outline, CLI_LINESZ,
	         "STACK words err:%lu can:%lu rtd:%lu apps:%lu",
	         task_stack_words(data->error_task),
	         task_stack_words(data->canbus_task),
	         task_stack_words(data->rtd_task),
	         task_stack_words(data->apps_task));
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ,
	         "STACK words bse:%lu bppc:%lu cool:%lu cli:%lu",
	         task_stack_words(data->bse_task),
	         task_stack_words(data->bppc_task),
	         task_stack_words(data->cool_task),
	         task_stack_words(data->cli_task));
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ,
	         "STACK words acc:%lu dash:%lu",
	         task_stack_words(data->acc_task),
	         task_stack_words(data->dashboard_task));
	cli_printline(cli, outline);
	return 0;
}


int get_power_status(int argc, char *argv[])
{
	int16_t target_torque_0p1nm;
	int16_t command_torque_0p1nm;
	uint8_t reason;
	uint8_t authority_state;
	uint16_t steady_calls;
	uint16_t transition_calls;
	uint16_t cells_evaluated;
	uint32_t deadline_overruns;
	uint32_t clamp_last_cycles;
	uint32_t clamp_max_cycles;
	uint32_t clamp_soft_overruns;
	uint32_t clamp_consecutive_overruns;
	bool clamp_overrun_fault;
	uint32_t residual_violations;
	uint32_t source_epoch;
	bool output_valid;
	bool residual_fault;
	ecu_torque_clamp_state_t clamp_state;
	ecu_current_prediction_snapshot_t prediction;
	bool calibration_qualified;
	uint32_t calibration_generation;
	int32_t predicted_min_0p1a;
	int32_t predicted_max_0p1a;

	(void)argc;
	(void)argv;

	taskENTER_CRITICAL();
	target_torque_0p1nm = data->cm200_target_torque_0p1nm;
	command_torque_0p1nm = data->cm200_command_torque_0p1nm;
	reason = data->torque_clamp_reason;
	authority_state = data->battery_authority_state;
	steady_calls = data->torque_clamp_steady_calls;
	transition_calls = data->torque_clamp_transition_calls;
	cells_evaluated = data->torque_clamp_cells_evaluated;
	deadline_overruns = data->torque_clamp_deadline_overrun_count;
	clamp_last_cycles = data->torque_clamp_last_cycles;
	clamp_max_cycles = data->torque_clamp_max_cycles;
	clamp_soft_overruns = data->torque_clamp_soft_overrun_count;
	clamp_consecutive_overruns =
		data->torque_clamp_consecutive_overruns;
	clamp_overrun_fault = data->torque_clamp_overrun_fault;
	output_valid = data->torque_clamp_output_valid;
	residual_fault = data->current_model_residual_fault;
	residual_violations = data->current_residual_violation_count;
	source_epoch = data->current_source_epoch;
	clamp_state = data->torque_clamp_state;
	prediction = data->current_prediction;
	calibration_qualified = data->pack_current_calibration_runtime.qualified;
	calibration_generation = data->pack_current_calibration_runtime.generation;
	taskEXIT_CRITICAL();

	predicted_min_0p1a = (int32_t)(prediction.predicted_pack_current_a.min_a * 10.0f);
	predicted_max_0p1a = (int32_t)(prediction.predicted_pack_current_a.max_a * 10.0f);

	snprintf(outline, CLI_LINESZ,
	         "PWR tgt:%d cmd:%d reason:%u auth:%u valid:%u",
	         (int)target_torque_0p1nm, (int)command_torque_0p1nm,
	         (unsigned)reason, (unsigned)authority_state,
	         (unsigned)output_valid);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ,
	         "PWR path:%u sign:%u phase:%u active:%u zero:%u calls:%u/%u cells:%u",
	         (unsigned)clamp_state.path_state,
	         (unsigned)clamp_state.last_nonzero_committed_sign,
	         (unsigned)clamp_state.monitor_phase,
	         (unsigned)clamp_state.transition.active,
	         (unsigned)clamp_state.physical_zero_confirmed,
	         (unsigned)steady_calls, (unsigned)transition_calls,
	         (unsigned)cells_evaluated);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ,
	         "PWR pred:%ld..%ld dA pred_valid:%u residual:%u/%lu epoch:%lu",
	         (long)predicted_min_0p1a, (long)predicted_max_0p1a,
	         (unsigned)prediction.valid, (unsigned)residual_fault,
	         (unsigned long)residual_violations, (unsigned long)source_epoch);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ,
	         "PWR cal:%u gen:%lu hard:%lu soft:%lu consec:%lu fault:%u",
	         (unsigned)calibration_qualified,
	         (unsigned long)calibration_generation,
	         (unsigned long)deadline_overruns,
	         (unsigned long)clamp_soft_overruns,
	         (unsigned long)clamp_consecutive_overruns,
	         (unsigned)clamp_overrun_fault);
	cli_printline(cli, outline);
	snprintf(outline, CLI_LINESZ,
	         "PWR WCET cycles last/max:%lu/%lu us:%lu/%lu",
	         (unsigned long)clamp_last_cycles,
	         (unsigned long)clamp_max_cycles,
	         (unsigned long)stm32f767_cycles_to_us(clamp_last_cycles),
	         (unsigned long)stm32f767_cycles_to_us(clamp_max_cycles));
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

static bool cli_datetime_fields_valid(int month, int day, int year,
                                      int hour, int minute, int second)
{
    static const uint8_t days_per_month[12] =
    {
        31u, 28u, 31u, 30u, 31u, 30u,
        31u, 31u, 30u, 31u, 30u, 31u
    };
    int max_day;

    if((year < 0) || (year > 99) ||
       (month < 1) || (month > 12) ||
       (hour < 0) || (hour > 23) ||
       (minute < 0) || (minute > 59) ||
       (second < 0) || (second > 59))
    {
        return false;
    }

    max_day = (int)days_per_month[month - 1];
    /* STM32 RTC year 00-99 is used as 2000-2099 by this firmware. */
    if((month == 2) && ((year % 4) == 0))
    {
        max_day = 29;
    }

    return (day >= 1) && (day <= max_day);
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

    if(!cli_datetime_fields_valid(month, day, year, hour, minute, second))
    {
        cli_printline(cli, "ERROR: RTC fields are outside valid ranges");
        return 1;
    }

	data->datetime.month = (uint16_t)month;
	data->datetime.day = (uint16_t)day;
	data->datetime.year = (uint16_t)year;
	data->datetime.hour = (uint16_t)hour;
	data->datetime.minute = (uint16_t)minute;
	data->datetime.second = (uint16_t)second;
	if(write_time() != HAL_OK)
    {
        cli_printline(cli, "ERROR: RTC write failed");
        return 1;
    }

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
	snprintf(outline, CLI_LINESZ, "  cm200:   %d", data->cm200_fault);
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
