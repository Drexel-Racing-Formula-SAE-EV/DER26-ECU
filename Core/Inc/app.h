/**
* @file app.h
* @author Cole Bardin (cab572@drexel.edu)
* @brief
* @version 0.1
* @date 2023-03-13
*
* @copyright Copyright (c) 2023
*
*/

#ifndef __APP_H_
#define __APP_H_

#include <stdbool.h>
#include <stdint.h>

#include "main.h"
#include "board.h"
#include "ecu_config.h"
#include "ext_drivers/rtc.h"
#include "ext_drivers/ecu_safety.h"

#define VER_MAJOR 2
#define VER_MINOR 3
#define VER_BUG   0

#define PLAUSIBILITY_THRESH 10
#define BRAKE_LIGHT_THRESH 5
#define BPPC_BSE_THRESH 10
#define BPPC_APPS_H_THRESH 25
#define BPPC_APPS_L_THRESH 5

#define COOLANT_FLOW_MIN 5.0f

#define ERR_FREQ 20
#define APPS_FREQ 20
#define BSE_FREQ 20
#define BPPC_FREQ 20
#define CLI_FREQ 5
#define ACC_FREQ 5
#define DASH_FREQ 5
#define COOL_FREQ 5
#define RTD_FREQ 5

#define ERR_PRIO 17
#define CLI_PRIO 16
#define RTD_PRIO 15
#define CAN_PRIO 14
#define APPS_PRIO 10
#define BPPC_PRIO 8
#define BSE_PRIO 7
#define ACC_PRIO 5
#define DASH_PRIO 4
#define COOL_PRIO 3

#define MAXTRQ 200 // maximum nM of toruqe that will be requested from motorcontroller (=100% throttle)

typedef struct {
	int throttle;
	int brake;

	rtd_state_t rtd_mode;

	bool hard_fault;
	bool soft_fault;
	
	bool coolant_fault;
	bool apps_fault;
	bool bse_fault;
	bool bppc_fault;
	bool acc_fault;
	bool cli_fault;
	bool canbus_fault;
	bool canbus_rx_fault;
	bool canbus_tx_fault;
	bool canbus_hw_fault;
	bool ams_fault;
	bool dashboard_fault;
	bool mq_fault;
	
	bool fw_state;
	bool tsal;
	bool rtd_button;
	bool cascadia_ok;
	bool cascadia_error;
	bool cascadia_en;
	bool cascadia_on;
	bool imd_fail;
	bool bms_fail;
	bool bspd_fail;
	bool bspd_ok_raw;
	bool startup_fault;
	bool task_heartbeat_fault;
	bool rtd_trip_pulse_requested;
	uint32_t can_error_code;
	uint32_t can_rx_overrun_count;
	uint32_t can_recovery_count;
	uint32_t reset_cause;
	uint8_t cm200_rolling_counter;
	volatile uint32_t apps_heartbeat_tick;
	volatile uint32_t bse_heartbeat_tick;
	volatile uint32_t bppc_heartbeat_tick;
	volatile uint32_t rtd_heartbeat_tick;
	volatile uint32_t can_heartbeat_tick;

	bool brakelight;

	float coolant_pressure;
	float coolant_flow;
	float coolant_temp_in;
	float coolant_temp_out;
	bool coolant_telemetry_valid;

	board_t board;
	datetime_t datetime;

	TaskHandle_t dev_task;
	TaskHandle_t cli_task;
	TaskHandle_t rtd_task;
	TaskHandle_t error_task;
	TaskHandle_t apps_task;
	TaskHandle_t bse_task;
	TaskHandle_t bppc_task;
	TaskHandle_t canbus_task;
	TaskHandle_t acc_task;
	TaskHandle_t dashboard_task;
	TaskHandle_t cool_task;
} app_data_t;

void app_create();
void cli_putline(char *line);
HAL_StatusTypeDef read_time();
HAL_StatusTypeDef write_time();
void set_ecu_ok(bool state);
void set_buzzer(bool state);
void set_cascadia_enable(bool state);
void set_cascadia_on(bool state);
void set_brakelight(bool state);
void set_ssa(int duty);
void ecu_force_safe_outputs(void);
void ecu_watchdog_init(void);
void ecu_watchdog_refresh(void);

#endif
