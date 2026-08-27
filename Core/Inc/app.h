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
#include "power/ecu_current_residual_monitor.h"
#include "power/ecu_pack_current_model.h"
#include "power/ecu_torque_clamp.h"

#define VER_MAJOR 2
#define VER_MINOR 10
#define VER_BUG   7

#define DER26_CAN_CONTRACT_NAME "DER26-CAN-V4"

/* DER26-CAN-V4 nominal vehicle bitrate. The asynchronous AMS scheduler is
 * required to remain correct at 250 kbps; 500 kbps is vehicle headroom. */
#ifndef DER26_CAN_BITRATE_KBPS
#define DER26_CAN_BITRATE_KBPS 500u
#endif
#if DER26_CAN_BITRATE_KBPS == 500u
#define DER26_CAN_PRESCALER 6u
#elif DER26_CAN_BITRATE_KBPS == 250u
#define DER26_CAN_PRESCALER 12u
#else
#error "DER26 CAN supports only validated 250 or 500 kbps timing"
#endif
#define DER26_CAN_SJW CAN_SJW_2TQ
#define DER26_CAN_BS1 CAN_BS1_15TQ
#define DER26_CAN_BS2 CAN_BS2_2TQ

#define PLAUSIBILITY_THRESH 10
#define BRAKE_LIGHT_THRESH 5
#define BPPC_BSE_THRESH 10
#define BPPC_APPS_H_THRESH 25
#define BPPC_APPS_L_THRESH 5

#define COOLANT_FLOW_MIN 5.0f

#define ERR_FREQ 100
#define APPS_FREQ 100
#define BSE_FREQ 100
#define BPPC_FREQ 20
#define CLI_FREQ 5
#define ACC_FREQ 5
#define DASH_FREQ 5
#define COOL_FREQ 5
#define RTD_FREQ 50

/* Static task stack budgets, in 32-bit StackType_t words on Cortex-M7. */
#define ECU_STACK_ERROR_WORDS      256u
#define ECU_STACK_RTD_WORDS        256u
#define ECU_STACK_CAN_WORDS        768u
#define ECU_STACK_APPS_WORDS       768u
#define ECU_STACK_BPPC_WORDS       256u
#define ECU_STACK_BSE_WORDS        256u
#define ECU_STACK_CLI_WORDS       1024u
#define ECU_STACK_ACC_WORDS        256u
#define ECU_STACK_DASH_WORDS       256u
#define ECU_STACK_COOL_WORDS       256u
#define ECU_STACK_LOGGER_WORDS    1536u

#define ERR_PRIO 17
#define RTD_PRIO 15
#define CAN_PRIO 14
#define APPS_PRIO 10
#define BPPC_PRIO 8
#define BSE_PRIO 7
#define CLI_PRIO 6
#define ACC_PRIO 5
#define DASH_PRIO 4
#define COOL_PRIO 3
#define LOGGER_PRIO 2

#define MAXTRQ 200 /* Maximum requested motoring torque in Nm at 100% APPS. */

typedef struct {
	volatile int throttle;
	volatile int brake;

	volatile rtd_state_t rtd_mode;

	volatile bool hard_fault;
	volatile bool soft_fault;
	
	volatile bool coolant_fault;
	volatile bool apps_fault;
	volatile bool bse_fault;
	volatile bool bppc_fault;
	volatile bool acc_fault;
	volatile bool cli_fault;
	volatile bool canbus_fault;
	volatile bool canbus_rx_fault;
	volatile bool canbus_tx_fault;
	volatile bool canbus_hw_fault;
	volatile bool ams_fault;
	volatile bool cm200_fault;
	volatile bool cm200_ready;
	volatile bool cm200_feedback_seen;
	volatile bool cm200_startup_timeout;
	volatile bool cm200_runtime_fault_latched;
	volatile bool dashboard_fault;
	volatile bool mq_fault;
	
	volatile bool fw_state;
	volatile bool tsal;
	volatile bool rtd_button;
	volatile bool cascadia_ok;
	volatile bool cascadia_error;
	volatile bool cascadia_en;
	volatile bool cascadia_on;
	volatile bool imd_fail;
	volatile bool bms_fail;
	volatile bool bspd_fail;
	volatile bool bspd_ok_raw;
	volatile bool startup_fault;
	volatile bool task_heartbeat_fault;
	volatile bool rtd_trip_pulse_requested;
	volatile uint32_t can_error_code;
	volatile uint32_t can_rx_overrun_count;
	/* Target-only CAN RX ISR timing/backlog evidence. These counters are
	 * diagnostic and never grant torque authority. */
	volatile uint32_t can_rx_isr_callback_count;
	volatile uint32_t can_rx_isr_last_cycles;
	volatile uint32_t can_rx_isr_max_cycles;
	volatile uint32_t can_rx_isr_budget_exhaust_count;
	volatile uint32_t can_recovery_count;
	uint32_t reset_cause;
	volatile uint8_t cm200_rolling_counter;
	volatile int16_t cm200_target_torque_0p1nm;
	volatile int16_t cm200_command_torque_0p1nm;
	volatile uint8_t torque_clamp_reason;
	volatile uint16_t torque_clamp_steady_calls;
	volatile uint16_t torque_clamp_transition_calls;
	volatile uint16_t torque_clamp_cells_evaluated;
	volatile uint32_t torque_clamp_deadline_overrun_count;
	volatile uint32_t torque_clamp_consecutive_overruns;
	volatile bool torque_clamp_overrun_fault;
	volatile bool torque_clamp_output_valid;
	volatile bool current_model_residual_fault;
	volatile uint8_t battery_authority_state;
	volatile uint32_t current_residual_violation_count;
	volatile uint32_t current_source_epoch;
	volatile uint32_t current_measurement_sequence;
	volatile uint32_t torque_clamp_last_cycles;
	volatile uint32_t torque_clamp_max_cycles;
	volatile uint32_t torque_clamp_soft_overrun_count;
	/* Final CAN commit critical-section timing. This includes the last AMS/CM200
	 * authority re-read, comparison-only clamp verification and non-blocking
	 * HAL CAN mailbox enqueue. It is target evidence only and never grants
	 * torque authority. */
	volatile uint32_t torque_commit_count;
	volatile uint32_t torque_commit_last_cycles;
	volatile uint32_t torque_commit_max_cycles;
	ecu_pack_current_calibration_runtime_t pack_current_calibration_runtime;
	ecu_torque_clamp_state_t torque_clamp_state;
	ecu_current_residual_monitor_t current_residual_monitor;
	ecu_current_prediction_snapshot_t current_prediction;
	volatile int16_t ams_cm200_voltage_delta_0p1v;
	volatile bool ams_cm200_voltage_crosscheck_valid;
	volatile bool ams_cm200_voltage_mismatch;
	volatile uint32_t apps_heartbeat_tick;
	volatile uint32_t bse_heartbeat_tick;
	volatile uint32_t bppc_heartbeat_tick;
	volatile uint32_t rtd_heartbeat_tick;
	volatile uint32_t can_heartbeat_tick;
	volatile uint32_t cool_heartbeat_tick;

	volatile bool brakelight;

	float coolant_pressure;
	float coolant_flow;
	float coolant_temp_in;
	float coolant_temp_out;
	bool coolant_telemetry_valid;
	volatile uint16_t coolant_fault_flags;
	volatile uint8_t coolant_pump_mode; /* 0=auto, 1=failsafe max, 2=manual */
	float coolant_pump_manual_percent;
	float coolant_pump_command_percent;
	float coolant_pump_s_duty_percent;
	float coolant_pump_gate_duty_percent;
	volatile uint8_t coolant_pump_flags;

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
	TaskHandle_t logger_task;
} app_data_t;

extern app_data_t app;

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
