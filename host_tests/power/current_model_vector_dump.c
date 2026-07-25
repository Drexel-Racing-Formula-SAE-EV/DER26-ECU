#include <stdio.h>
#include <string.h>

#include "power/ecu_pack_current_model.h"

static void add_transition(ecu_pack_current_calibration_t *cal,
                           ecu_transition_profile_t profile,
                           ecu_transition_direction_t direction,
                           float span, float min_a, float max_a)
{
    ecu_transition_current_cell_t *cell =
        &cal->transition_cells[cal->transition_cell_count++];
    cell->profile = profile;
    cell->direction = direction;
    cell->span_max_nm = span;
    cell->region_count = 1u;
    cell->regions[0] = (ecu_transition_operating_region_t){
        .speed_min_rpm = -20000.0f, .speed_max_rpm = 20000.0f,
        .vdc_min_v = 100.0f, .vdc_max_v = 400.0f,
        .inverter_temp_min_c = -40.0f, .inverter_temp_max_c = 150.0f,
        .motor_temp_min_c = -40.0f,
        .motor_temp_max_c = 150.0f,
        .absolute_pack_current_a = {min_a, max_a},
        .maximum_settling_time_us = (uint32_t)(20000.0f + span * 500.0f),
    };
}

static ecu_pack_current_calibration_t calibration(void)
{
    ecu_pack_current_calibration_t c;
    memset(&c, 0, sizeof(c));
    c.magic = ECU_CURRENT_MODEL_MAGIC;
    c.schema_version = ECU_CURRENT_MODEL_SCHEMA_VERSION;
    c.torque_axis_points = 5u;
    c.steady_cell_count = 4u;
    c.torque_axis_nm[0] = -100.0f;
    c.torque_axis_nm[1] = 0.0f;
    c.torque_axis_nm[2] = 50.0f;
    c.torque_axis_nm[3] = 100.0f;
    c.torque_axis_nm[4] = 200.0f;
    const ecu_current_interval_t intervals[4] = {
        {-20.0f, 10.0f}, {0.0f, 25.0f},
        {10.0f, 55.0f}, {20.0f, 110.0f},
    };
    for(uint16_t i = 0u; i < 4u; ++i)
    {
        c.steady_cells[i].torque_min_nm = c.torque_axis_nm[i];
        c.steady_cells[i].torque_max_nm = c.torque_axis_nm[i + 1u];
        c.steady_cells[i].region_count = 1u;
        c.steady_cells[i].regions[0] = (ecu_steady_operating_region_t){
            .speed_min_rpm = -20000.0f, .speed_max_rpm = 20000.0f,
            .vdc_min_v = 100.0f, .vdc_max_v = 400.0f,
            .inverter_temp_min_c = -40.0f, .inverter_temp_max_c = 150.0f,
            .motor_temp_min_c = -40.0f,
            .motor_temp_max_c = 150.0f,
            .current_a = intervals[i],
        };
    }
    const ecu_transition_profile_t profiles[2] = {
        ECU_TRANSITION_PROFILE_SLEW_LIMITED,
        ECU_TRANSITION_PROFILE_COMPOSED,
    };
    const ecu_transition_direction_t directions[3] = {
        ECU_TRANSITION_DIRECTION_FROM_ZERO,
        ECU_TRANSITION_DIRECTION_SAME_SIGN_INCREASE,
        ECU_TRANSITION_DIRECTION_SAME_SIGN_DECREASE,
    };
    for(unsigned p = 0u; p < 2u; ++p)
    {
        for(unsigned d = 0u; d < 3u; ++d)
        {
            add_transition(&c, profiles[p], directions[d], 50.0f, -20.0f, 45.0f);
            add_transition(&c, profiles[p], directions[d], 100.0f, -25.0f, 70.0f);
            add_transition(&c, profiles[p], directions[d], 200.0f, -35.0f, 120.0f);
        }
    }
    add_transition(&c, ECU_TRANSITION_PROFILE_ZERO_ASSERT,
                   ECU_TRANSITION_DIRECTION_TO_ZERO, 200.0f, -40.0f, 130.0f);
    add_transition(&c, ECU_TRANSITION_PROFILE_UNKNOWN_TO_ZERO,
                   ECU_TRANSITION_DIRECTION_TO_ZERO, 200.0f, -40.0f, 130.0f);
    add_transition(&c, ECU_TRANSITION_PROFILE_REVERSAL_TO_ZERO,
                   ECU_TRANSITION_DIRECTION_REVERSAL_FIRST_LEG,
                   200.0f, -45.0f, 135.0f);

    c.certified_speed_min_rpm = -20000.0f;
    c.certified_speed_max_rpm = 20000.0f;
    c.certified_vdc_min_v = 100.0f;
    c.certified_vdc_max_v = 400.0f;
    c.certified_inverter_temp_min_c = -40.0f;
    c.certified_inverter_temp_max_c = 150.0f;
    c.certified_motor_temp_min_c = -40.0f;
    c.certified_motor_temp_max_c = 150.0f;

    c.r2d_aux_current_min_a = 0.0f;
    c.r2d_aux_current_max_a = 0.25f;
    c.numeric_margin_negative_a = 0.001f;
    c.numeric_margin_positive_a = 0.001f;
    c.torque_uncertainty_negative_nm = 0.2f;
    c.torque_uncertainty_positive_nm = 0.3f;
    c.speed_sensor_uncertainty_negative_rpm = 10.0f;
    c.speed_sensor_uncertainty_positive_rpm = 15.0f;
    c.maximum_acceleration_rpm_per_s = 5000.0f;
    c.maximum_deceleration_rpm_per_s = 10000.0f;
    c.vdc_uncertainty_negative_v = 1.0f;
    c.vdc_uncertainty_positive_v = 2.0f;
    c.inverter_temperature_uncertainty_negative_c = 2.0f;
    c.motor_temperature_uncertainty_negative_c = 2.0f;
    c.inverter_temperature_uncertainty_positive_c = 3.0f;
    c.motor_temperature_uncertainty_positive_c = 3.0f;
    c.maximum_speed_age_us = 100000u;
    c.maximum_vdc_age_us = 100000u;
    c.maximum_inverter_temperature_age_us = 200000u;
    c.maximum_motor_temperature_age_us = 200000u;
    c.zero_enter_nm = 0.5f;
    c.zero_exit_nm = 1.0f;
    c.tracking_band_nm = 1.0f;
    c.maximum_microstep_nm = 0.5f;
    c.maximum_settled_command_rate_nm_per_s = 50.0f;
    c.maximum_anchor_deviation_nm = 2.0f;
    c.maximum_cumulative_drift_nm = 4.0f;
    c.settled_tracking_time_us = 20000u;
    c.steady_confirmation_samples = 3u;
    c.microstep_margin_negative_a = 0.1f;
    c.microstep_margin_positive_a = 0.2f;
    c.late_zero_transition_current_a = (ecu_current_interval_t){-50.0f, 140.0f};
    c.late_zero_maximum_settling_time_us = 150000u;
    c.low_margin_consumption_fraction = 0.8f;
    c.minimum_discharge_headroom_a = 5.0f;
    c.minimum_charge_headroom_a = 5.0f;
    c.nonzero_torque_threshold_nm = 0.5f;
    c.evidence_valid = true;
    c.crc32 = ecu_pack_current_calibration_crc32(&c);
    return c;
}

int main(void)
{
    ecu_pack_current_calibration_t cal = calibration();
    ecu_pack_current_calibration_runtime_t runtime;
    if(!ecu_pack_current_calibration_qualify(&cal, 9u, &runtime))
    {
        return 2;
    }

    const float torques[] = {-75.0f, -0.1f, 0.0f, 25.0f, 49.9f,
                             50.0f, 75.0f, 150.0f};
    for(unsigned i = 0u; i < sizeof(torques) / sizeof(torques[0]); ++i)
    {
        ecu_steady_current_input_t input = {
            .raw_torque_nm = torques[i],
            .motor_speed_rpm = (i == 2u) ? 0.0f : 3000.0f,
            .dc_bus_voltage_v = 300.0f,
            .inverter_temp_c = 30.0f,
            .motor_temp_c = 30.0f,
            .motor_speed_age_us = (uint32_t)(i * 1000u),
            .dc_bus_voltage_age_us = 1000u,
            .inverter_temp_age_us = 2000u,
            .motor_temp_age_us = 2000u,
            .apply_microstep_margin = (i & 1u) != 0u,
        };
        ecu_steady_current_output_t output;
        ecu_pack_current_evaluate_steady(&input, &runtime, &output);
        printf("S,%.6f,%.6f,%.6f,%.6f,%.6f,%u,%u,%u,%u,%u,%d,%.6f,%.6f,%u,%u\n",
               input.raw_torque_nm, input.motor_speed_rpm,
               input.dc_bus_voltage_v, input.inverter_temp_c,
               input.motor_temp_c, input.motor_speed_age_us,
               input.dc_bus_voltage_age_us, input.inverter_temp_age_us,
               input.motor_temp_age_us,
               input.apply_microstep_margin ? 1u : 0u,
               (int)output.status, output.pack_current_a.min_a,
               output.pack_current_a.max_a, output.torque_cells_evaluated,
               output.regions_evaluated);
    }

    const float spans[] = {20.0f, 50.0f, 80.0f, 150.0f};
    for(unsigned i = 0u; i < sizeof(spans) / sizeof(spans[0]); ++i)
    {
        ecu_transition_current_input_t input = {
            .settled_anchor_torque_nm = 0.0f,
            .minimum_raw_commanded_torque_nm = 0.0f,
            .maximum_raw_commanded_torque_nm = spans[i],
            .latest_raw_target_torque_nm = spans[i],
            .motor_speed_rpm = 3000.0f,
            .dc_bus_voltage_v = 300.0f,
            .inverter_temp_c = 30.0f,
            .motor_temp_c = 30.0f,
            .motor_speed_age_us = 1000u,
            .dc_bus_voltage_age_us = 1000u,
            .inverter_temp_age_us = 1000u,
            .motor_temp_age_us = 1000u,
            .profile = ECU_TRANSITION_PROFILE_SLEW_LIMITED,
            .direction = ECU_TRANSITION_DIRECTION_FROM_ZERO,
        };
        ecu_transition_current_output_t output;
        ecu_pack_current_evaluate_transition(&input, &runtime, &output);
        printf("T,%.6f,%d,%d,%d,%.6f,%.6f,%u,%u\n",
               spans[i], (int)input.profile, (int)input.direction,
               (int)output.status, output.absolute_pack_current_a.min_a,
               output.absolute_pack_current_a.max_a,
               output.maximum_settling_time_us, output.regions_evaluated);
    }
    return 0;
}
