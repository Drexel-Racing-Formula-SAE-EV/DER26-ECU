/*
 * ecu_safety.h
 *
 * Small pure-logic helpers for ECU safety state machines. These functions are
 * intentionally independent of HAL/FreeRTOS so they can be exercised by host
 * SIL tests before touching the STM32 board.
 */

#ifndef __ECU_SAFETY_H_
#define __ECU_SAFETY_H_

#include <stdbool.h>
#include <stdint.h>

#define ECU_CM200_DATALEN 8u
#define ECU_CM200_DISABLE_UNLOCK_CYCLES 5u
#define ECU_RTD_BUZZ_TIME_MS 3000u
#define ECU_RTD_TRIP_PULSE_MS 100u

typedef enum {
    RTD_AWAIT_TSAL,
    RTD_AWAIT_BUTTON_FALSE,
    RTD_AWAIT_CONDITIONS,
    RTD_BUZZING,
    RTD_ENABLED
} rtd_state_t;

typedef struct
{
    bool hard_fault;
    bool apps_fault;
    bool bse_fault;
    bool bppc_fault;
    bool ams_fault;
    bool canbus_fault;
    bool canbus_rx_fault;
    bool canbus_tx_fault;
    bool imd_fail;
    bool bms_fail;
    bool bspd_fail;
} ecu_fault_inputs_t;

typedef struct
{
    bool tsal;
    bool rtd_button;
    bool cascadia_ok;
    bool brakelight;
    ecu_fault_inputs_t faults;
} ecu_rtd_inputs_t;

typedef struct
{
    rtd_state_t state;
    uint32_t buzz_start_tick;
    bool buzzer_on;
    bool trip_pulse_requested;
} ecu_rtd_step_t;

typedef struct
{
    bool cascadia_ok;
    bool hard_fault;
    bool apps_fault;
    bool bppc_fault;
    bool bse_fault;
    bool ams_fault;
    bool canbus_fault;
    bool canbus_rx_fault;
    bool canbus_tx_fault;
    bool imd_fail;
    bool bms_fail;
    bool bspd_fail;
    rtd_state_t rtd_mode;
} ecu_torque_inputs_t;

typedef struct
{
    bool initialized;
    bool faulted;
    uint8_t healthy_samples;
} ecu_discrete_filter_t;

bool ecu_faults_clear(const ecu_fault_inputs_t *faults);
bool ecu_rtd_conditions_met(const ecu_rtd_inputs_t *inputs);
ecu_rtd_step_t ecu_rtd_step(rtd_state_t state,
                            uint32_t buzz_start_tick,
                            const ecu_rtd_inputs_t *inputs,
                            uint32_t now_ms);

bool ecu_torque_allowed(const ecu_torque_inputs_t *inputs);
void ecu_discrete_filter_init(ecu_discrete_filter_t *filter);
bool ecu_discrete_fault_update(ecu_discrete_filter_t *filter,
                               bool raw_fault,
                               uint8_t clear_samples);
bool ecu_bspd_raw_is_fault(bool raw_pin_high);
void ecu_cm200_build_disable_packet(uint8_t data[ECU_CM200_DATALEN]);
void ecu_cm200_build_torque_packet(uint8_t data[ECU_CM200_DATALEN], int16_t torque_cmd);
void ecu_cm200_apply_rolling_counter(uint8_t data[ECU_CM200_DATALEN], uint8_t rolling_counter);
uint8_t ecu_cm200_next_rolling_counter(uint8_t rolling_counter);
bool ecu_cm200_update_unlock(bool torque_allowed, uint8_t *disable_unlock_cycles);
bool ecu_heartbeat_expired(uint32_t last_tick, uint32_t now_tick, uint32_t max_age_ms);

#endif /* __ECU_SAFETY_H_ */
