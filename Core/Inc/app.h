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

#ifndef ECU_APP_H_
#define ECU_APP_H_

#include <stdbool.h>
#include <stdint.h>

#include "main.h"
#include "board.h"
#include "ext_drivers/rtc.h"

#define VER_MAJOR 2u
#define VER_MINOR 2u
#define VER_BUG   0u

#define PLAUSIBILITY_THRESH 10u
#define BRAKE_LIGHT_THRESH 5u
#define BPPC_BSE_THRESH 10u
#define BPPC_APPS_H_THRESH 25u
#define BPPC_APPS_L_THRESH 5u

#define COOLANT_FLOW_MIN 5.0f

#define ERR_FREQ 20u
#define APPS_FREQ 20u
#define BSE_FREQ 20u
#define BPPC_FREQ 20u
#define CLI_FREQ 5u
#define ACC_FREQ 5u
#define DASH_FREQ 5u
#define COOL_FREQ 5u
#define RTD_FREQ 5u

#define ERR_PRIO 17u
#define CLI_PRIO 16u
#define RTD_PRIO 15u
#define CAN_PRIO 14u
#define APPS_PRIO 10u
#define BPPC_PRIO 8u
#define BSE_PRIO 7u
#define ACC_PRIO 5u
#define DASH_PRIO 4u
#define COOL_PRIO 3u

#define MAXTRQ 200u // maximum nM of toruqe that will be requested from motorcontroller (=100% throttle)

typedef enum {
	RTD_AWAIT_TSAL,
	RTD_AWAIT_BUTTON_FALSE,
	RTD_AWAIT_CONDITIONS,
	RTD_ENABLED
} rtd_state_t;

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
	bool ams_fault;
	bool dashboard_fault;
	bool mq_fault;
	
	bool fw_state;
	bool fw_override;
	bool fw_override_state;
	bool tsal;
	bool rtd_button;
	bool cascadia_ok;
	bool cascadia_error;
	bool cascadia_en;
	bool cascadia_on;
	bool imd_fail;
	bool bms_fail;
	bool bspd_fail;

	bool brakelight;

	float coolant_pressure;
	float coolant_flow;
	float coolant_temp_in;
	float coolant_temp_out;

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

void app_create(void);
void cli_putline(char *line);
HAL_StatusTypeDef read_time(void);
HAL_StatusTypeDef write_time(void);
void set_ecu_ok(bool state);
void override_ecu_ok(bool state);
void apply_ecu_ok_override(bool state);
void set_buzzer(bool state);
void set_cascadia_enable(bool state);
void set_cascadia_on(bool state);
void set_brakelight(bool state);
void set_ssa(int duty);

#endif
