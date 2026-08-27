/*
 * cooling_control.h
 * Pure conversion, plausibility and pump-command logic for the DER26 ECU.
 * This module owns no GPIO and no torque authority.
 */
#ifndef INC_EXT_DRIVERS_COOLING_CONTROL_H_
#define INC_EXT_DRIVERS_COOLING_CONTROL_H_

#include <stdbool.h>
#include <stdint.h>

#define ECU_COOLING_FAULT_TEMP_IN_SENSOR   (1u << 0)
#define ECU_COOLING_FAULT_TEMP_OUT_SENSOR  (1u << 1)
#define ECU_COOLING_FAULT_PRESS_SENSOR     (1u << 2)
#define ECU_COOLING_FAULT_FLOW_STALE       (1u << 3)
#define ECU_COOLING_FAULT_OVER_TEMP        (1u << 4)
#define ECU_COOLING_FAULT_LOW_FLOW         (1u << 5)
#define ECU_COOLING_FAULT_OVER_PRESSURE    (1u << 6)
#define ECU_COOLING_FAULT_TEMP_DELTA       (1u << 7)

#define ECU_COOLING_PUMP_FLAG_VALID_PWM    (1u << 0)
#define ECU_COOLING_PUMP_FLAG_FAILSAFE_MAX (1u << 1)
#define ECU_COOLING_PUMP_FLAG_MANUAL       (1u << 2)

typedef struct
{
    float temp_in_c;
    float temp_out_c;
    float pressure_psi;
    float flow_lpm;
    float pump_command_pct;
    bool temp_in_valid;
    bool temp_out_valid;
    bool pressure_valid;
    bool flow_valid;
} ecu_cooling_sample_t;

typedef struct
{
    uint16_t sample_count;
    uint8_t bad_count;
    uint8_t good_count;
    bool fault;
    uint16_t fault_flags;
} ecu_cooling_monitor_t;

typedef struct
{
    float requested_pct;
    float pump_s_duty_pct;
    float mcu_gate_duty_pct;
    uint8_t flags;
} ecu_coolant_pump_command_t;

float ecu_coolant_temp_sen04_5_from_adc(uint16_t count, bool *valid);
float ecu_coolant_pressure_100psi_from_adc(uint16_t count, bool *valid);
float ecu_coolant_flow_bv2000_from_hz(float frequency_hz, bool input_valid,
                                      bool *valid);
void ecu_cooling_monitor_init(ecu_cooling_monitor_t *monitor);
bool ecu_cooling_monitor_update(ecu_cooling_monitor_t *monitor,
                                const ecu_cooling_sample_t *sample);
ecu_coolant_pump_command_t ecu_coolant_pump_command(float requested_pct,
                                                     bool failsafe_max,
                                                     bool manual);

#endif /* INC_EXT_DRIVERS_COOLING_CONTROL_H_ */
