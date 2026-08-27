#include "power/ecu_pack_current_calibration.h"

/* Deliberately invalid checked-in calibration. The full schema is populated
 * with finite conservative placeholders so tooling can inspect it, but zero
 * cells, false evidence, and a zero CRC guarantee qualification fails. A
 * separately generated and reviewed artifact is required for vehicle use. */
const ecu_pack_current_calibration_t g_ecu_pack_current_calibration = {
    .magic = ECU_CURRENT_MODEL_MAGIC,
    .schema_version = ECU_CURRENT_MODEL_SCHEMA_VERSION,
    .torque_axis_points = 0u,
    .steady_cell_count = 0u,
    .transition_cell_count = 0u,

    .certified_speed_min_rpm = -20000.0f,
    .certified_speed_max_rpm = 20000.0f,
    .certified_vdc_min_v = 100.0f,
    .certified_vdc_max_v = 400.0f,
    .certified_inverter_temp_min_c = -40.0f,
    .certified_inverter_temp_max_c = 150.0f,
    .certified_motor_temp_min_c = -40.0f,
    .certified_motor_temp_max_c = 180.0f,

    .r2d_aux_current_min_a = 0.0f,
    .r2d_aux_current_max_a = 0.250f,
    .numeric_margin_negative_a = 0.001f,
    .numeric_margin_positive_a = 0.001f,

    .torque_uncertainty_negative_nm = 0.0f,
    .torque_uncertainty_positive_nm = 0.0f,
    .speed_sensor_uncertainty_negative_rpm = 0.0f,
    .speed_sensor_uncertainty_positive_rpm = 0.0f,
    .maximum_acceleration_rpm_per_s = 0.0f,
    .maximum_deceleration_rpm_per_s = 0.0f,
    .vdc_uncertainty_negative_v = 0.0f,
    .vdc_uncertainty_positive_v = 0.0f,
    .inverter_temperature_uncertainty_negative_c = 0.0f,
    .inverter_temperature_uncertainty_positive_c = 0.0f,
    .motor_temperature_uncertainty_negative_c = 0.0f,
    .motor_temperature_uncertainty_positive_c = 0.0f,
    .maximum_speed_age_us = 0u,
    .maximum_vdc_age_us = 0u,
    .maximum_inverter_temperature_age_us = 0u,
    .maximum_motor_temperature_age_us = 0u,

    .zero_enter_nm = 0.5f,
    .zero_exit_nm = 1.0f,
    .tracking_band_nm = 1.0f,
    .maximum_microstep_nm = 0.5f,
    .maximum_settled_command_rate_nm_per_s = 50.0f,
    .maximum_anchor_deviation_nm = 2.0f,
    .maximum_cumulative_drift_nm = 4.0f,
    .settled_tracking_time_us = 50000u,
    .steady_confirmation_samples = 3u,
    .microstep_margin_negative_a = 0.0f,
    .microstep_margin_positive_a = 0.0f,

    .late_zero_transition_current_a = {-200.0f, 200.0f},
    .late_zero_maximum_settling_time_us = 500000u,

    .low_margin_consumption_fraction = 0.80f,
    .minimum_discharge_headroom_a = 5.0f,
    .minimum_charge_headroom_a = 5.0f,
    .nonzero_torque_threshold_nm = 0.5f,

    .evidence_valid = false,
    .crc32 = 0u,
};
