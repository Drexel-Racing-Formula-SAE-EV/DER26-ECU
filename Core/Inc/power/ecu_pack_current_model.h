#ifndef ECU_PACK_CURRENT_MODEL_H_
#define ECU_PACK_CURRENT_MODEL_H_

#include <stdbool.h>
#include <stdint.h>

#define ECU_CURRENT_MODEL_SCHEMA_VERSION 3u
#define ECU_CURRENT_MODEL_MAX_TORQUE_AXIS_POINTS 21u
#define ECU_CURRENT_MODEL_MAX_TORQUE_CELLS \
    (ECU_CURRENT_MODEL_MAX_TORQUE_AXIS_POINTS - 1u)
#define ECU_CURRENT_MODEL_MAX_OPERATING_REGIONS_PER_CELL 4u
#define ECU_CURRENT_MODEL_MAX_TORQUE_REGIONS_PER_POINT 4u
#define ECU_CURRENT_MODEL_MAX_TOTAL_REGIONS_PER_POINT 16u
#define ECU_CURRENT_MODEL_MAX_TRANSITION_CELLS 64u
#define ECU_CURRENT_MODEL_MAX_TRANSITION_REGIONS_PER_CELL 4u

#define ECU_CURRENT_MODEL_MAGIC 0x4550434Du /* 'EPCM' */

typedef enum
{
    ECU_CURRENT_MODEL_INVALID = 0,
    ECU_CURRENT_MODEL_OK,
    ECU_CURRENT_MODEL_OUT_OF_DOMAIN,
    ECU_CURRENT_MODEL_REGION_OVERFLOW,
    ECU_CURRENT_MODEL_NUMERIC_FAULT,
    ECU_CURRENT_MODEL_UNCALIBRATED
} ecu_current_model_status_t;

typedef enum
{
    ECU_TRANSITION_PROFILE_INVALID = 0,
    ECU_TRANSITION_PROFILE_HOLD,
    ECU_TRANSITION_PROFILE_MICROSTEP,
    ECU_TRANSITION_PROFILE_SLEW_LIMITED,
    ECU_TRANSITION_PROFILE_IMMEDIATE_STEP,
    ECU_TRANSITION_PROFILE_ZERO_ASSERT,
    ECU_TRANSITION_PROFILE_REVERSAL_TO_ZERO,
    ECU_TRANSITION_PROFILE_UNKNOWN_TO_ZERO,
    ECU_TRANSITION_PROFILE_COMPOSED
} ecu_transition_profile_t;

typedef enum
{
    ECU_TRANSITION_DIRECTION_INVALID = 0,
    ECU_TRANSITION_DIRECTION_HOLD,
    ECU_TRANSITION_DIRECTION_SAME_SIGN_INCREASE,
    ECU_TRANSITION_DIRECTION_SAME_SIGN_DECREASE,
    ECU_TRANSITION_DIRECTION_TO_ZERO,
    ECU_TRANSITION_DIRECTION_FROM_ZERO,
    ECU_TRANSITION_DIRECTION_REVERSAL_FIRST_LEG
} ecu_transition_direction_t;

typedef struct
{
    float min_a;
    float max_a;
} ecu_current_interval_t;

typedef struct
{
    float speed_min_rpm;
    float speed_max_rpm;
    float vdc_min_v;
    float vdc_max_v;
    float inverter_temp_min_c;
    float inverter_temp_max_c;
    float motor_temp_min_c;
    float motor_temp_max_c;
    ecu_current_interval_t current_a;
} ecu_steady_operating_region_t;

typedef struct
{
    float torque_min_nm;
    float torque_max_nm;
    uint8_t region_count;
    ecu_steady_operating_region_t
        regions[ECU_CURRENT_MODEL_MAX_OPERATING_REGIONS_PER_CELL];
} ecu_steady_current_cell_t;

typedef struct
{
    float speed_min_rpm;
    float speed_max_rpm;
    float vdc_min_v;
    float vdc_max_v;
    float inverter_temp_min_c;
    float inverter_temp_max_c;
    float motor_temp_min_c;
    float motor_temp_max_c;
    ecu_current_interval_t absolute_pack_current_a;
    uint32_t maximum_settling_time_us;
} ecu_transition_operating_region_t;

typedef struct
{
    ecu_transition_profile_t profile;
    ecu_transition_direction_t direction;
    float span_max_nm;
    uint8_t region_count;
    ecu_transition_operating_region_t
        regions[ECU_CURRENT_MODEL_MAX_TRANSITION_REGIONS_PER_CELL];
} ecu_transition_current_cell_t;

typedef struct
{
    uint32_t magic;
    uint16_t schema_version;
    uint16_t torque_axis_points;
    uint16_t steady_cell_count;
    uint16_t transition_cell_count;

    float torque_axis_nm[ECU_CURRENT_MODEL_MAX_TORQUE_AXIS_POINTS];
    ecu_steady_current_cell_t
        steady_cells[ECU_CURRENT_MODEL_MAX_TORQUE_CELLS];
    ecu_transition_current_cell_t
        transition_cells[ECU_CURRENT_MODEL_MAX_TRANSITION_CELLS];

    /* Complete operating domain covered by every steady torque cell and by
     * every transition profile/direction/span cell. Boot qualification proves
     * the region rectangles cover this domain without gaps; runtime then only
     * needs bounded intersection/union work. */
    float certified_speed_min_rpm;
    float certified_speed_max_rpm;
    float certified_vdc_min_v;
    float certified_vdc_max_v;
    float certified_inverter_temp_min_c;
    float certified_inverter_temp_max_c;
    float certified_motor_temp_min_c;
    float certified_motor_temp_max_c;

    /* Healthy Ready-to-Drive current crossing the canonical pack-current
     * boundary. The lower bound is normally zero so regeneration receives no
     * credit from an unmeasured auxiliary load. */
    float r2d_aux_current_min_a;
    float r2d_aux_current_max_a;

    /* Numerical representation/rounding margins. These widen the interval;
     * safety comparisons themselves are made directly against zero/DCL/CCL. */
    float numeric_margin_negative_a;
    float numeric_margin_positive_a;

    /* Centrally owned input uncertainty and freshness contract. */
    float torque_uncertainty_negative_nm;
    float torque_uncertainty_positive_nm;
    float speed_sensor_uncertainty_negative_rpm;
    float speed_sensor_uncertainty_positive_rpm;
    float maximum_acceleration_rpm_per_s;
    float maximum_deceleration_rpm_per_s;
    float vdc_uncertainty_negative_v;
    float vdc_uncertainty_positive_v;
    float inverter_temperature_uncertainty_negative_c;
    float inverter_temperature_uncertainty_positive_c;
    float motor_temperature_uncertainty_negative_c;
    float motor_temperature_uncertainty_positive_c;
    uint32_t maximum_speed_age_us;
    uint32_t maximum_vdc_age_us;
    uint32_t maximum_inverter_temperature_age_us;
    uint32_t maximum_motor_temperature_age_us;

    /* Torque classification/tracking contract. */
    float zero_enter_nm;
    float zero_exit_nm;
    float tracking_band_nm;
    float maximum_microstep_nm;
    float maximum_settled_command_rate_nm_per_s;
    float maximum_anchor_deviation_nm;
    float maximum_cumulative_drift_nm;
    uint32_t settled_tracking_time_us;
    uint16_t steady_confirmation_samples;
    float microstep_margin_negative_a;
    float microstep_margin_positive_a;

    /* Bounded fallback used when a late commit-time tightening converts a
     * previously selected nonzero command into an immediate zero. */
    ecu_current_interval_t late_zero_transition_current_a;
    uint32_t late_zero_maximum_settling_time_us;

    /* Battery-authority classification thresholds. */
    float low_margin_consumption_fraction;
    float minimum_discharge_headroom_a;
    float minimum_charge_headroom_a;
    float nonzero_torque_threshold_nm;

    bool evidence_valid;
    uint32_t crc32;
} ecu_pack_current_calibration_t;

typedef struct
{
    const ecu_pack_current_calibration_t *calibration;
    uint32_t qualified_crc32;
    uint32_t generation;
    bool qualified;
} ecu_pack_current_calibration_runtime_t;

typedef struct
{
    float raw_torque_nm;
    float motor_speed_rpm;
    float dc_bus_voltage_v;
    float inverter_temp_c;
    float motor_temp_c;
    uint32_t motor_speed_age_us;
    uint32_t dc_bus_voltage_age_us;
    uint32_t inverter_temp_age_us;
    uint32_t motor_temp_age_us;
    bool apply_microstep_margin;
} ecu_steady_current_input_t;

typedef struct
{
    ecu_current_interval_t pack_current_a;
    ecu_current_model_status_t status;
    uint16_t torque_cells_evaluated;
    uint16_t regions_evaluated;
    bool low_speed_or_zero_crossing;
    bool output_valid;
} ecu_steady_current_output_t;

typedef struct
{
    float settled_anchor_torque_nm;
    float minimum_raw_commanded_torque_nm;
    float maximum_raw_commanded_torque_nm;
    float latest_raw_target_torque_nm;
    float motor_speed_rpm;
    float dc_bus_voltage_v;
    float inverter_temp_c;
    float motor_temp_c;
    uint32_t motor_speed_age_us;
    uint32_t dc_bus_voltage_age_us;
    uint32_t inverter_temp_age_us;
    uint32_t motor_temp_age_us;
    ecu_transition_profile_t profile;
    ecu_transition_direction_t direction;
} ecu_transition_current_input_t;

typedef struct
{
    ecu_current_interval_t absolute_pack_current_a;
    ecu_current_model_status_t status;
    uint32_t maximum_settling_time_us;
    uint16_t regions_evaluated;
    bool output_valid;
} ecu_transition_current_output_t;

uint32_t ecu_pack_current_calibration_crc32(
    const ecu_pack_current_calibration_t *cal);
bool ecu_pack_current_calibration_validate_full(
    const ecu_pack_current_calibration_t *cal);
bool ecu_pack_current_calibration_qualify(
    const ecu_pack_current_calibration_t *cal,
    uint32_t generation,
    ecu_pack_current_calibration_runtime_t *runtime);
bool ecu_pack_current_calibration_runtime_valid(
    const ecu_pack_current_calibration_runtime_t *runtime);

/* Compatibility alias for tooling/tests that explicitly request a full boot
 * validation. Runtime control code shall use a qualified runtime handle. */
static inline bool ecu_pack_current_calibration_validate(
    const ecu_pack_current_calibration_t *cal)
{
    return ecu_pack_current_calibration_validate_full(cal);
}

ecu_current_model_status_t ecu_pack_current_evaluate_steady(
    const ecu_steady_current_input_t *input,
    const ecu_pack_current_calibration_runtime_t *runtime,
    ecu_steady_current_output_t *output);

ecu_current_model_status_t ecu_pack_current_evaluate_steady_cell(
    uint16_t torque_cell_index,
    const ecu_steady_current_input_t *input,
    const ecu_pack_current_calibration_runtime_t *runtime,
    ecu_steady_current_output_t *output);

ecu_current_model_status_t ecu_pack_current_evaluate_transition(
    const ecu_transition_current_input_t *input,
    const ecu_pack_current_calibration_runtime_t *runtime,
    ecu_transition_current_output_t *output);

#endif
