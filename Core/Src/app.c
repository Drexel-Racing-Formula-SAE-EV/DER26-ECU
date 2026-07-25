/**
* @file app.c
* @author Cole Bardin (cab572@drexel.edu)
* @brief
* @version 0.1
* @date 2023-03-19
*
* @copyright Copyright (c) 2023
*
*/
#include <string.h>

#include "app.h"
#include "tasks/bse_task.h"
#include "tasks/rtd_task.h"
#include "tasks/error_task.h"
#include "tasks/bse_task.h"
#include "tasks/bppc_task.h"
#include "tasks/apps_task.h"
#include "tasks/canbus_task.h"
#include "tasks/cli_task.h"
#include "tasks/acc_task.h"
#include "tasks/dashboard_task.h"
#include "tasks/cool_task.h"
#include "power/ecu_pack_current_calibration.h"

app_data_t app = {0};

void ecu_force_safe_outputs(void)
{
    /* Direct BSRR writes are deterministic and do not depend on scheduler or
     * HAL state. They are also harmless if invoked before GPIO clocks start. */
    Firmware_Ok_GPIO_Port->BSRR = ((uint32_t)Firmware_Ok_Pin << 16u);
    MTR_EN_GPIO_Port->BSRR = ((uint32_t)MTR_EN_Pin << 16u);
    Cascadia_ON_GPIO_Port->BSRR = ((uint32_t)Cascadia_ON_Pin << 16u);
    Buzzer_GPIO_Port->BSRR = ((uint32_t)Buzzer_Pin << 16u);
}

void ecu_watchdog_init(void)
{
#if ECU_ENABLE_IWDG
    IWDG->KR = 0x5555u; /* Enable PR/RLR writes. */
    IWDG->PR = ECU_IWDG_PRESCALER_REGISTER;
    IWDG->RLR = ECU_IWDG_RELOAD_REGISTER;
    IWDG->KR = 0xAAAAu; /* Load the counter. */
    IWDG->KR = 0xCCCCu; /* Start; cannot be stopped until reset. */
#endif
}

void ecu_watchdog_refresh(void)
{
#if ECU_ENABLE_IWDG
    IWDG->KR = 0xAAAAu;
#endif
}

void app_create()
{
	app.reset_cause = RCC->CSR;
	__HAL_RCC_CLEAR_RESET_FLAGS();
	app.throttle = 0;
	app.brake = 0;

	app.rtd_mode = RTD_AWAIT_TSAL;

	app.hard_fault = false;
	app.soft_fault = false;

	app.coolant_fault = false;
	/* Safety-related inputs start failed and are cleared only by their tasks. */
	app.apps_fault = true;
	app.bse_fault = true;
	app.bppc_fault = false;
	app.acc_fault = false;
	app.cli_fault = false;
	app.canbus_fault = false;
	app.canbus_rx_fault = false;
	app.canbus_tx_fault = false;
	app.canbus_hw_fault = false;
	app.ams_fault = true;
	app.cm200_fault = true;
	app.cm200_ready = false;
	app.cm200_feedback_seen = false;
	app.cm200_startup_timeout = false;
	app.cm200_runtime_fault_latched = false;
	app.dashboard_fault = false;
	app.mq_fault = false;

	app.fw_state = false;
	app.tsal = false;
	app.rtd_button = false;
	app.cascadia_ok = false;
	app.cascadia_error = false;
	app.cascadia_en = false;
	app.imd_fail = true;
	app.bms_fail = true;
	app.bspd_fail = true;
	app.bspd_ok_raw = false;
	app.startup_fault = true;
	app.task_heartbeat_fault = false;
	app.rtd_trip_pulse_requested = false;
	app.cm200_rolling_counter = 0u;
	app.cm200_target_torque_0p1nm = 0;
	app.cm200_command_torque_0p1nm = 0;
	app.torque_clamp_deadline_overrun_count = 0u;
	app.ams_cm200_voltage_delta_0p1v = 0;
	app.ams_cm200_voltage_crosscheck_valid = false;
	app.ams_cm200_voltage_mismatch = false;

	/* The checked-in calibration is intentionally invalid, so qualification
	 * normally leaves the runtime handle unqualified until a separately
	 * reviewed release calibration is linked. Runtime model calls never CRC the
	 * calibration again. */
	ecu_torque_clamp_state_init(&app.torque_clamp_state);
	ecu_current_residual_monitor_init(&app.current_residual_monitor);
	memset(&app.current_prediction, 0, sizeof(app.current_prediction));
	app.current_model_residual_fault = false;
	app.current_residual_violation_count = 0u;
	/* Automatic mid-run source switching is prohibited in this release. A boot
	 * creates one source epoch; a future source-diagnostic CAN frame can replace
	 * this fixed boot epoch when controlled runtime switching is certified. */
	app.current_source_epoch = 1u;
	app.current_measurement_sequence = 0u;
	app.torque_clamp_last_cycles = 0u;
	app.torque_clamp_max_cycles = 0u;
	app.torque_clamp_soft_overrun_count = 0u;
	app.battery_authority_state =
		(uint8_t)ECU_BATTERY_AUTHORITY_TORQUE_EXHAUSTED;
	(void)ecu_pack_current_calibration_qualify(
		&g_ecu_pack_current_calibration, 1u,
		&app.pack_current_calibration_runtime);

	app.brakelight = false;

	app.coolant_pressure = 0.0;
	app.coolant_flow = 0.0;
	app.coolant_temp_in = 0.0;
	app.coolant_temp_out = 0.0;
	app.coolant_telemetry_valid = false;

	app.throttle = 0;
	app.brake = 0;

	board_init(&app.board);
	ecu_force_safe_outputs();
	set_cascadia_enable(0);
	set_cascadia_on(0);

	app.cli_fault = (HAL_UART_Receive_IT(app.board.cli.huart,
	                                    (uint8_t *)&app.board.cli.c,
	                                    1) != HAL_OK);
	if(app.board.canbus.started)
	{
		app.canbus_hw_fault =
			(HAL_CAN_ActivateNotification(app.board.canbus.hcan,
			     CAN_IT_RX_FIFO0_MSG_PENDING |
			     CAN_IT_RX_FIFO0_OVERRUN |
			     CAN_IT_ERROR_WARNING |
			     CAN_IT_ERROR_PASSIVE |
			     CAN_IT_BUSOFF |
			     CAN_IT_LAST_ERROR_CODE |
			     CAN_IT_ERROR) != HAL_OK);
	}
	else
	{
		app.canbus_hw_fault = true;
	}

//	HAL_Delay(2);

	app.cli_task = cli_task_start(&app);
	app.rtd_task = rtd_task_start(&app);
	app.error_task = error_task_start(&app);
	app.canbus_task = canbus_task_start(&app);
	app.bse_task = bse_task_start(&app);
	app.apps_task = apps_task_start(&app);
	app.bppc_task = bppc_task_start(&app);
	app.acc_task = acc_task_start(&app);
	app.dashboard_task = dashboard_task_start(&app);
	app.cool_task = cool_task_start(&app);

	app.startup_fault = ((app.cli_task == NULL) ||
	                     (app.rtd_task == NULL) ||
	                     (app.error_task == NULL) ||
	                     (app.canbus_task == NULL) ||
	                     (app.bse_task == NULL) ||
	                     (app.apps_task == NULL) ||
	                     (app.bppc_task == NULL) ||
	                     (app.acc_task == NULL) ||
	                     (app.dashboard_task == NULL) ||
	                     (app.cool_task == NULL) ||
	                     !app.board.canbus.started ||
	                     app.canbus_hw_fault ||
	                     !app.board.stm32f767.initialized);

}

HAL_StatusTypeDef read_time(){
	RTC_TimeTypeDef rTime;
	RTC_DateTypeDef rDate;
	HAL_StatusTypeDef ret = 0;

	ret |= HAL_RTC_GetTime(app.board.stm32f767.hrtc, &rTime, RTC_FORMAT_BIN);
	ret |= HAL_RTC_GetDate(app.board.stm32f767.hrtc, &rDate, RTC_FORMAT_BIN);

	app.datetime.second = rTime.Seconds;
	app.datetime.minute = rTime.Minutes;
	app.datetime.hour = rTime.Hours;
	app.datetime.day = rDate.Date;
	app.datetime.month = rDate.Month;
	app.datetime.year = rDate.Year;

	return ret;
}

HAL_StatusTypeDef write_time(){
	RTC_TimeTypeDef rTime;
	RTC_DateTypeDef rDate;
	HAL_StatusTypeDef ret = 0;
	RTC_HandleTypeDef *rtc = app.board.stm32f767.hrtc;

	rTime.Seconds = HEX2DEC(app.datetime.second);
	rTime.Minutes = HEX2DEC(app.datetime.minute);
	rTime.Hours = HEX2DEC(app.datetime.hour);
	rDate.Date = HEX2DEC(app.datetime.day);
	rDate.Month = HEX2DEC(app.datetime.month);
	rDate.Year = HEX2DEC(app.datetime.year);
	rDate.WeekDay = RTC_WEEKDAY_MONDAY;

	ret |= HAL_RTC_SetTime(rtc, &rTime, RTC_FORMAT_BCD);
	ret |= HAL_RTC_SetDate(rtc, &rDate, RTC_FORMAT_BCD);

	return ret;
}

void set_ecu_ok(bool state)
{
	app.fw_state = state;
	HAL_GPIO_WritePin(Firmware_Ok_GPIO_Port, Firmware_Ok_Pin, state);
}

void set_buzzer(bool state)
{
	HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, state);
}

void set_cascadia_enable(bool state)
{
	app.cascadia_en = state;
	HAL_GPIO_WritePin(MTR_EN_GPIO_Port, MTR_EN_Pin, state);
}

void set_cascadia_on(bool state)
{
	app.cascadia_on = state;
	HAL_GPIO_WritePin(Cascadia_ON_GPIO_Port, Cascadia_ON_Pin, state);
}

void set_brakelight(bool state)
{
	app.brakelight = state;
	HAL_GPIO_WritePin(Brake_Light_GPIO_Port, Brake_Light_Pin, state);
}

void set_ssa(int duty)
{
	if(duty > 100) duty = 100;
	else if(duty < 0) duty = 0;
	pwm_set_percent(&app.board.ssa, duty);
}
