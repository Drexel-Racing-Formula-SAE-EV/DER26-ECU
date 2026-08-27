/*
 * cm200.h
 *
 * Pure decoder and supervision state for Cascadia Motion CM200 broadcast CAN
 * messages.  The byte layouts and scale factors follow manual 0A-0163-04.
 * This module deliberately has no HAL or RTOS dependency so it can be fault-
 * injected in host tests.
 */

#ifndef __CM200_H_
#define __CM200_H_

#include <stdbool.h>
#include <stdint.h>

#define CM200_FRAME_DLC 8u

#define CM200_TEMPERATURES_1_CAN_ID 0x0A0u
#define CM200_TEMPERATURES_2_CAN_ID 0x0A1u
#define CM200_TEMPERATURES_3_CAN_ID 0x0A2u
#define CM200_MOTOR_POSITION_CAN_ID 0x0A5u
#define CM200_CURRENT_CAN_ID        0x0A6u
#define CM200_VOLTAGE_CAN_ID        0x0A7u
#define CM200_INTERNAL_STATES_CAN_ID 0x0AAu
#define CM200_FAULTS_CAN_ID         0x0ABu
#define CM200_TORQUE_TIMER_CAN_ID   0x0ACu
#define CM200_FIRMWARE_CAN_ID       0x0AEu
#define CM200_TORQUE_CAP_CAN_ID     0x0B1u

/* Default broadcasts are 100 Hz (fast) and 10 Hz (slow).  These limits allow
 * scheduling jitter and occasional arbitration while still failing well before
 * the CM200 command watchdog's configurable 500 ms maximum. */
#define CM200_FAST_STALE_TIMEOUT_MS       250u
#define CM200_SLOW_STALE_TIMEOUT_MS       750u
#define CM200_FIRMWARE_STALE_TIMEOUT_MS  2000u
#define CM200_COMMAND_STALE_TIMEOUT_MS    100u
#define CM200_INTEGRITY_MISMATCH_LIMIT      3u
#define CM200_TIMER_REPEAT_LIMIT             5u

typedef enum
{
    CM200_FRAME_TEMPERATURES_1 = 0,
    CM200_FRAME_TEMPERATURES_2,
    CM200_FRAME_TEMPERATURES_3,
    CM200_FRAME_MOTOR_POSITION,
    CM200_FRAME_CURRENT,
    CM200_FRAME_VOLTAGE,
    CM200_FRAME_INTERNAL_STATES,
    CM200_FRAME_FAULTS,
    CM200_FRAME_TORQUE_TIMER,
    CM200_FRAME_FIRMWARE,
    CM200_FRAME_TORQUE_CAPABILITY,
    CM200_FRAME_COUNT
} cm200_frame_index_t;

typedef struct
{
    uint32_t last_rx_tick;
    uint32_t rx_count;
    bool valid;
    bool sane;
    bool stale;
} cm200_frame_health_t;

typedef struct
{
    cm200_frame_health_t frame[CM200_FRAME_COUNT];

    /* 0x0A0, 0x0A1, 0x0A2: temperature values are 0.1 deg C/count. */
    int16_t module_a_temp_0p1c;
    int16_t module_b_temp_0p1c;
    int16_t module_c_temp_0p1c;
    int16_t gate_driver_temp_0p1c;
    int16_t control_board_temp_0p1c;
    int16_t rtd1_temp_0p1c;
    int16_t rtd2_temp_0p1c;
    int16_t motor_hotspot_temp_0p1c;
    int16_t coolant_temp_0p1c;
    int16_t inverter_hotspot_temp_0p1c;
    int16_t motor_temp_0p1c;
    int16_t torque_shudder_0p1nm;

    /* 0x0A5. */
    int16_t motor_angle;
    int16_t motor_speed_rpm;
    int16_t electrical_frequency_0p1hz;
    int16_t resolver_delta;

    /* 0x0A6: 0.1 A/count. */
    int16_t phase_a_current_0p1a;
    int16_t phase_b_current_0p1a;
    int16_t phase_c_current_0p1a;
    int16_t dc_bus_current_0p1a;

    /* 0x0A7: 0.1 V/count. */
    int16_t dc_bus_voltage_0p1v;
    int16_t output_voltage_0p1v;
    int16_t vd_voltage_0p1v;
    int16_t vq_voltage_0p1v;

    /* 0x0AA. */
    uint8_t vsm_state;
    uint8_t pwm_frequency_khz;
    uint8_t inverter_state;
    uint8_t relay_states;
    uint8_t mode_states;
    uint8_t command_states;
    uint8_t enable_states;
    uint8_t limit_states;
    uint8_t inverter_expected_counter;
    bool torque_mode;
    bool command_mode_can;
    bool inverter_enabled;
    bool inverter_enable_lockout;
    bool forward_direction;
    bool bms_active;

    /* 0x0AB. */
    uint32_t post_faults;
    uint32_t run_faults;

    /* 0x0AC. */
    int16_t commanded_torque_0p1nm;
    int16_t torque_feedback_0p1nm;
    uint32_t power_on_timer;
    uint32_t previous_power_on_timer;
    uint8_t timer_repeat_count;
    bool timer_observed_progress;
    bool timer_reset_fault;

    /* 0x0AE. */
    uint16_t eeprom_project_code;
    uint16_t software_version;
    uint16_t date_mmdd;
    uint16_t date_year;

    /* 0x0B1. */
    int16_t motor_torque_available_0p1nm;
    int16_t regen_torque_available_0p1nm;

    /* Command/broadcast integrity correlation. */
    bool have_command_tx;
    bool have_previous_command_tx;
    uint8_t last_command_counter;
    uint8_t next_expected_counter;
    int16_t last_command_torque_0p1nm;
    int16_t previous_command_torque_0p1nm;
    bool last_command_enabled;
    uint32_t last_command_tx_tick;
    uint32_t command_tx_count;
    bool command_tx_stale;
    bool counter_synced;
    bool counter_fault;
    uint8_t counter_mismatch_count;
    bool have_counter_observation;
    uint8_t last_observed_expected_counter;
    uint8_t counter_progress_count;
    bool torque_echo_synced;
    bool torque_echo_fault;
    uint8_t torque_echo_mismatch_count;

    uint32_t rx_count;
    uint32_t bad_rx_count;
} cm200_t;

void cm200_init(cm200_t *dev);
bool cm200_is_known_can_id(uint32_t std_id);
void cm200_invalidate_can_frame(cm200_t *dev, uint32_t std_id);
bool cm200_parse_can_frame(cm200_t *dev,
                           uint32_t std_id,
                           bool is_standard,
                           uint8_t dlc,
                           const uint8_t *data,
                           uint32_t now_ms);
void cm200_update_stale(cm200_t *dev, uint32_t now_ms);
void cm200_note_command_tx(cm200_t *dev,
                           uint8_t rolling_counter,
                           bool enabled,
                           int16_t torque_0p1nm,
                           uint32_t now_ms);
bool cm200_feedback_healthy(const cm200_t *dev);
bool cm200_allows_torque(const cm200_t *dev);
bool cm200_has_immediate_fault(const cm200_t *dev);
int16_t cm200_clamp_motoring_torque(const cm200_t *dev, int16_t requested_0p1nm);
uint32_t cm200_frame_age_ms(const cm200_t *dev, cm200_frame_index_t frame, uint32_t now_ms);

#endif /* __CM200_H_ */
