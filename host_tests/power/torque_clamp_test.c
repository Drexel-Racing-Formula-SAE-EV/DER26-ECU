#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "power/ecu_pack_current_model.h"
#include "power/ecu_torque_clamp.h"

static void add_transition(ecu_pack_current_calibration_t *cal,
                           ecu_transition_profile_t profile,
                           ecu_transition_direction_t direction,
                           float span,
                           float minimum_a,
                           float maximum_a,
                           uint32_t settling_us)
{
    assert(cal->transition_cell_count < ECU_CURRENT_MODEL_MAX_TRANSITION_CELLS);
    ecu_transition_current_cell_t *cell =
        &cal->transition_cells[cal->transition_cell_count++];
    cell->profile = profile;
    cell->direction = direction;
    cell->span_max_nm = span;
    cell->region_count = 1u;
    cell->regions[0] = (ecu_transition_operating_region_t){
        .speed_min_rpm = -20000.0f,
        .speed_max_rpm = 20000.0f,
        .vdc_min_v = 100.0f,
        .vdc_max_v = 400.0f,
        .inverter_temp_min_c = -40.0f,
        .inverter_temp_max_c = 150.0f,
        .motor_temp_min_c = -40.0f,
        .motor_temp_max_c = 150.0f,
        .absolute_pack_current_a = {minimum_a, maximum_a},
        .maximum_settling_time_us = settling_us,
    };
}

static void add_transition_family(ecu_pack_current_calibration_t *cal,
                                  ecu_transition_profile_t profile,
                                  ecu_transition_direction_t direction)
{
    add_transition(cal, profile, direction, 50.0f, -20.0f, 45.0f, 30000u);
    add_transition(cal, profile, direction, 100.0f, -25.0f, 70.0f, 50000u);
    add_transition(cal, profile, direction, 200.0f, -35.0f, 120.0f, 80000u);
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

    const ecu_current_interval_t current[4] = {
        {-20.0f, 10.0f},
        {0.0f, 25.0f},
        {10.0f, 55.0f},
        {20.0f, 110.0f},
    };
    for(uint16_t i = 0u; i < c.steady_cell_count; ++i)
    {
        ecu_steady_current_cell_t *cell = &c.steady_cells[i];
        cell->torque_min_nm = c.torque_axis_nm[i];
        cell->torque_max_nm = c.torque_axis_nm[i + 1u];
        cell->region_count = 1u;
        cell->regions[0] = (ecu_steady_operating_region_t){
            .speed_min_rpm = -20000.0f,
            .speed_max_rpm = 20000.0f,
            .vdc_min_v = 100.0f,
            .vdc_max_v = 400.0f,
            .inverter_temp_min_c = -40.0f,
            .inverter_temp_max_c = 150.0f,
            .motor_temp_min_c = -40.0f,
            .motor_temp_max_c = 150.0f,
            .current_a = current[i],
        };
    }

    add_transition_family(&c, ECU_TRANSITION_PROFILE_SLEW_LIMITED,
                          ECU_TRANSITION_DIRECTION_FROM_ZERO);
    add_transition_family(&c, ECU_TRANSITION_PROFILE_SLEW_LIMITED,
                          ECU_TRANSITION_DIRECTION_SAME_SIGN_INCREASE);
    add_transition_family(&c, ECU_TRANSITION_PROFILE_SLEW_LIMITED,
                          ECU_TRANSITION_DIRECTION_SAME_SIGN_DECREASE);
    add_transition_family(&c, ECU_TRANSITION_PROFILE_COMPOSED,
                          ECU_TRANSITION_DIRECTION_FROM_ZERO);
    add_transition_family(&c, ECU_TRANSITION_PROFILE_COMPOSED,
                          ECU_TRANSITION_DIRECTION_SAME_SIGN_INCREASE);
    add_transition_family(&c, ECU_TRANSITION_PROFILE_COMPOSED,
                          ECU_TRANSITION_DIRECTION_SAME_SIGN_DECREASE);
    add_transition_family(&c, ECU_TRANSITION_PROFILE_ZERO_ASSERT,
                          ECU_TRANSITION_DIRECTION_TO_ZERO);
    add_transition_family(&c, ECU_TRANSITION_PROFILE_UNKNOWN_TO_ZERO,
                          ECU_TRANSITION_DIRECTION_TO_ZERO);
    add_transition_family(&c, ECU_TRANSITION_PROFILE_REVERSAL_TO_ZERO,
                          ECU_TRANSITION_DIRECTION_REVERSAL_FIRST_LEG);

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
    c.torque_uncertainty_negative_nm = 0.0f;
    c.torque_uncertainty_positive_nm = 0.0f;
    c.speed_sensor_uncertainty_negative_rpm = 10.0f;
    c.speed_sensor_uncertainty_positive_rpm = 10.0f;
    c.maximum_acceleration_rpm_per_s = 5000.0f;
    c.maximum_deceleration_rpm_per_s = 10000.0f;
    c.vdc_uncertainty_negative_v = 1.0f;
    c.vdc_uncertainty_positive_v = 1.0f;
    c.inverter_temperature_uncertainty_negative_c = 2.0f;
    c.motor_temperature_uncertainty_negative_c = 2.0f;
    c.inverter_temperature_uncertainty_positive_c = 2.0f;
    c.motor_temperature_uncertainty_positive_c = 2.0f;
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
    c.microstep_margin_positive_a = 0.1f;
    c.late_zero_transition_current_a = (ecu_current_interval_t){-50.0f, 140.0f};
    c.late_zero_maximum_settling_time_us = 100000u;
    c.low_margin_consumption_fraction = 0.80f;
    c.minimum_discharge_headroom_a = 5.0f;
    c.minimum_charge_headroom_a = 5.0f;
    c.nonzero_torque_threshold_nm = 0.5f;
    c.evidence_valid = true;
    c.crc32 = ecu_pack_current_calibration_crc32(&c);
    return c;
}

static ecu_pack_current_calibration_runtime_t qualify(
    ecu_pack_current_calibration_t *cal)
{
    ecu_pack_current_calibration_runtime_t runtime;
    assert(ecu_pack_current_calibration_qualify(cal, 7u, &runtime));
    assert(ecu_pack_current_calibration_runtime_valid(&runtime));
    return runtime;
}

static ecu_torque_clamp_input_t input(float request)
{
    return (ecu_torque_clamp_input_t){
        .requested_torque_nm = request,
        .motor_speed_rpm = 3000.0f,
        .dc_bus_voltage_v = 300.0f,
        .inverter_temp_c = 30.0f,
        .motor_temp_c = 30.0f,
        .cm200_positive_cap_nm = 200.0f,
        .cm200_negative_cap_nm = -100.0f,
        .dcl_a = 80.0f,
        .ccl_a = 40.0f,
        .now_us = 10000u,
        .motor_speed_age_us = 1000u,
        .dc_bus_voltage_age_us = 1000u,
        .inverter_temp_age_us = 1000u,
        .motor_temp_age_us = 1000u,
        .speed_generation = 10u,
        .vdc_generation = 20u,
        .temperature_generation = 30u,
        .capability_generation = 40u,
        .authority_received_ms = 9u,
        .calibration_generation = 7u,
        .authority_counter = 3u,
        .discharge_authorized = true,
        .charge_authorized = true,
        .authority_valid = true,
        .operating_point_valid = true,
        .physical_zero_confirmed = true,
    };
}

static ecu_torque_clamp_state_t confirmed_zero_state(void)
{
    ecu_torque_clamp_state_t state;
    ecu_torque_clamp_state_init(&state);
    state.path_state = ECU_TORQUE_PATH_CONFIRMED_ZERO;
    state.physical_zero_confirmed = true;
    state.normalized_zero = true;
    state.transition.settled_anchor_torque_nm = 0.0f;
    return state;
}

static void test_cell_aligned_path_cannot_skip_first_cell(void)
{
    ecu_pack_current_calibration_t cal = calibration();
    cal.steady_cells[1].regions[0].current_a.max_a = 100.0f;
    cal.steady_cells[2].regions[0].current_a.max_a = 20.0f;
    cal.crc32 = ecu_pack_current_calibration_crc32(&cal);
    ecu_pack_current_calibration_runtime_t runtime = qualify(&cal);
    ecu_torque_clamp_state_t state = confirmed_zero_state();
    ecu_torque_clamp_input_t in = input(80.0f);
    ecu_torque_clamp_output_t out;
    assert(ecu_torque_clamp_run(&in, &runtime, &state, &out));
    assert(out.output_valid);
    assert(out.selected_torque_nm == 0.0f);
    assert(out.torque_cells_evaluated >= 1u);
}

static void test_increase_stops_inside_last_feasible_cell(void)
{
    ecu_pack_current_calibration_t cal = calibration();
    /* [0,50) remains feasible; [50,100) is not. The returned command must
     * stay inside the certified first cell rather than landing on the shared
     * 50 Nm boundary and requiring the infeasible neighbor. */
    cal.steady_cells[2].regions[0].current_a.max_a = 100.0f;
    cal.crc32 = ecu_pack_current_calibration_crc32(&cal);
    ecu_pack_current_calibration_runtime_t runtime = qualify(&cal);
    ecu_torque_clamp_state_t state = confirmed_zero_state();
    ecu_torque_clamp_input_t in = input(80.0f);
    ecu_torque_clamp_output_t out;
    assert(ecu_torque_clamp_run(&in, &runtime, &state, &out));
    assert(out.output_valid);
    assert(out.selected_torque_nm > 49.0f);
    assert(out.selected_torque_nm < 50.0f);
    assert(out.steady_current_a.max_a < in.dcl_a);
}

static void test_transition_refinement(void)
{
    ecu_pack_current_calibration_t cal = calibration();
    ecu_pack_current_calibration_runtime_t runtime = qualify(&cal);
    ecu_torque_clamp_state_t state = confirmed_zero_state();
    ecu_torque_clamp_input_t in = input(80.0f);
    in.dcl_a = 60.0f; /* 100 Nm-span envelope is 70 A; 50 span is 45 A. */
    ecu_torque_clamp_output_t out;
    assert(ecu_torque_clamp_run(&in, &runtime, &state, &out));
    assert(out.selected_torque_nm > 0.0f);
    assert(out.selected_torque_nm <= 50.0f);
    assert(out.transition_refinement_iterations > 0u);
}

static void test_unknown_previous_uses_unknown_to_zero_profile(void)
{
    ecu_pack_current_calibration_t cal = calibration();
    ecu_pack_current_calibration_runtime_t runtime = qualify(&cal);
    ecu_torque_clamp_state_t state;
    ecu_torque_clamp_state_init(&state);
    ecu_torque_clamp_input_t in = input(20.0f);
    in.physical_zero_confirmed = false;
    ecu_torque_clamp_output_t out;
    assert(ecu_torque_clamp_run(&in, &runtime, &state, &out));
    assert(out.selected_torque_nm == 0.0f);
    assert(out.transition_profile == ECU_TRANSITION_PROFILE_UNKNOWN_TO_ZERO);
    assert(out.transition_direction == ECU_TRANSITION_DIRECTION_TO_ZERO);
}

static void test_reversal_zero_gate(void)
{
    ecu_pack_current_calibration_t cal = calibration();
    ecu_pack_current_calibration_runtime_t runtime = qualify(&cal);
    ecu_torque_clamp_state_t state = confirmed_zero_state();
    state.raw_committed_torque_nm = 10.0f;
    state.last_nonzero_committed_sign = ECU_TORQUE_SIGN_POSITIVE;
    state.path_state = ECU_TORQUE_PATH_WAITING_FOR_ZERO_CONFIRMATION;
    state.normalized_zero = true;
    state.physical_zero_confirmed = false;
    ecu_torque_clamp_input_t in = input(-20.0f);
    in.physical_zero_confirmed = false;
    ecu_torque_clamp_output_t out;
    assert(ecu_torque_clamp_run(&in, &runtime, &state, &out));
    assert(out.selected_torque_nm == 0.0f);
    assert(out.reason == ECU_CLAMP_REASON_REVERSAL_WAIT);
}

static void test_hold_with_unfinished_unauthorized_transition_goes_zero(void)
{
    ecu_pack_current_calibration_t cal = calibration();
    ecu_pack_current_calibration_runtime_t runtime = qualify(&cal);
    ecu_torque_clamp_state_t state = confirmed_zero_state();
    state.path_state = ECU_TORQUE_PATH_SAME_SIGN_TRACKING;
    state.raw_committed_torque_nm = 60.0f;
    state.normalized_zero = false;
    state.last_nonzero_committed_sign = ECU_TORQUE_SIGN_POSITIVE;
    state.transition.active = true;
    state.transition.settled_anchor_torque_nm = 0.0f;
    state.transition.minimum_raw_commanded_torque_nm = 0.0f;
    state.transition.maximum_raw_commanded_torque_nm = 60.0f;
    state.transition.latest_raw_target_torque_nm = 60.0f;
    state.transition.profile = ECU_TRANSITION_PROFILE_SLEW_LIMITED;
    state.transition.direction =
        ECU_TRANSITION_DIRECTION_SAME_SIGN_INCREASE;
    ecu_torque_clamp_input_t in = input(60.0f);
    in.dcl_a = 60.0f; /* steady is feasible; active 100 Nm span is not */
    ecu_torque_clamp_output_t out;
    assert(ecu_torque_clamp_run(&in, &runtime, &state, &out));
    assert(out.selected_torque_nm == 0.0f);
    assert(out.reason == ECU_CLAMP_REASON_TRANSITION_LIMIT);
}

static void test_transition_settles_and_reanchors(void)
{
    ecu_pack_current_calibration_t cal = calibration();
    ecu_pack_current_calibration_runtime_t runtime = qualify(&cal);
    ecu_torque_clamp_state_t state = confirmed_zero_state();
    ecu_torque_clamp_output_t candidate;
    memset(&candidate, 0, sizeof(candidate));
    candidate.selected_torque_nm = 20.0f;
    candidate.output_valid = true;
    candidate.selected_nonzero = true;
    candidate.material_change = true;
    candidate.transition_profile = ECU_TRANSITION_PROFILE_SLEW_LIMITED;
    candidate.transition_direction = ECU_TRANSITION_DIRECTION_FROM_ZERO;
    candidate.transition_maximum_settling_time_us = 30000u;

    ecu_torque_clamp_note_hardware_commit(&state, &candidate, 20.0f,
        ECU_CLAMP_REASON_NONE, 10000u, false, true, &runtime);
    assert(state.transition.active);
    assert(state.monitor_phase == ECU_CURRENT_MONITOR_SLEW_TRACKING);

    candidate.material_change = false;
    /* Command stability alone cannot establish steady state; one aligned
     * residual sample must also confirm the steady interval. */
    ecu_torque_clamp_note_hardware_commit(&state, &candidate, 20.0f,
        ECU_CLAMP_REASON_NONE, 40000u, false, false, &runtime);
    assert(state.transition.active);
    assert(state.transition.steady_confirmation_observed == 0u);
    for(uint32_t t = 50000u; t <= 70000u; t += 10000u)
    {
        ecu_torque_clamp_note_hardware_commit(&state, &candidate, 20.0f,
            ECU_CLAMP_REASON_NONE, t, false, true, &runtime);
    }
    assert(!state.transition.active);
    assert(state.monitor_phase == ECU_CURRENT_MONITOR_STEADY);
    assert(fabsf(state.transition.settled_anchor_torque_nm - 20.0f) < 0.001f);
    assert(state.transition.cumulative_raw_drift_nm == 0.0f);
}

static void test_microstep_drift_guard(void)
{
    ecu_pack_current_calibration_t cal = calibration();
    ecu_pack_current_calibration_runtime_t runtime = qualify(&cal);
    ecu_torque_clamp_state_t state = confirmed_zero_state();
    state.path_state = ECU_TORQUE_PATH_SAME_SIGN_TRACKING;
    state.raw_committed_torque_nm = 20.0f;
    state.normalized_zero = false;
    state.last_nonzero_committed_sign = ECU_TORQUE_SIGN_POSITIVE;
    state.transition.settled_anchor_torque_nm = 20.0f;
    state.transition.latest_raw_target_torque_nm = 20.0f;
    state.transition.last_commit_us = 10000u;
    state.monitor_phase = ECU_CURRENT_MONITOR_STEADY;

    ecu_torque_clamp_input_t in = input(20.4f);
    in.now_us = 20000u;
    ecu_torque_clamp_output_t out;
    assert(ecu_torque_clamp_run(&in, &runtime, &state, &out));
    assert(out.microstep_applied);
    ecu_torque_clamp_note_hardware_commit(&state, &out, out.selected_torque_nm,
        out.reason, in.now_us, false, true, &runtime);
    assert(state.transition.cumulative_raw_drift_nm > 0.39f);

    state.transition.cumulative_raw_drift_nm = 3.9f;
    in.requested_torque_nm = state.raw_committed_torque_nm + 0.4f;
    in.now_us += 10000u;
    assert(ecu_torque_clamp_run(&in, &runtime, &state, &out));
    assert(!out.microstep_applied);
    assert(out.material_change);
}

static void test_commit_reverify(void)
{
    ecu_torque_clamp_output_t candidate;
    memset(&candidate, 0, sizeof(candidate));
    candidate.selected_torque_nm = 20.0f;
    candidate.steady_current_a = (ecu_current_interval_t){0.0f, 10.0f};
    candidate.transition_current_a = (ecu_current_interval_t){0.0f, 15.0f};
    candidate.transition_interval_valid = true;
    candidate.transition_authority_required = true;
    candidate.output_valid = true;
    candidate.speed_generation = 1u;
    candidate.vdc_generation = 2u;
    candidate.temperature_generation = 3u;
    candidate.capability_generation = 4u;
    candidate.calibration_generation = 5u;
    candidate.computation_timestamp_us = 100000u;

    ecu_torque_commit_verification_t verify = {
        .dcl_a = 5.0f,
        .ccl_a = 20.0f,
        .cm200_positive_cap_nm = 100.0f,
        .cm200_negative_cap_nm = -100.0f,
        .speed_generation = 1u,
        .vdc_generation = 2u,
        .temperature_generation = 3u,
        .capability_generation = 4u,
        .calibration_generation = 5u,
        .commit_timestamp_us = 105000u,
        .discharge_authorized = true,
        .charge_authorized = true,
        .authority_valid = true,
        .calibration_valid = true,
        .safety_gate_valid = true,
    };
    float committed = 99.0f;
    ecu_torque_clamp_reason_t reason = ECU_CLAMP_REASON_NONE;
    assert(!ecu_torque_clamp_commit_reverify(&candidate, &verify,
                                              &committed, &reason));
    assert(committed == 0.0f);
    assert(reason == ECU_CLAMP_REASON_LATE_AUTHORITY_TIGHTENING);

    verify.dcl_a = 80.0f;
    verify.calibration_valid = false;
    assert(!ecu_torque_clamp_commit_reverify(&candidate, &verify,
                                              &committed, &reason));
    assert(committed == 0.0f);
    assert(reason == ECU_CLAMP_REASON_CALIBRATION_INVALID);
    verify.calibration_valid = true;

    verify.speed_generation++;
    assert(!ecu_torque_clamp_commit_reverify(&candidate, &verify,
                                              &committed, &reason));
    assert(reason == ECU_CLAMP_REASON_OPERATING_POINT_CHANGED);

    verify.speed_generation = candidate.speed_generation;
    verify.commit_timestamp_us =
        candidate.computation_timestamp_us + ECU_CLAMP_MAX_CONTRACT_AGE_US + 1u;
    assert(!ecu_torque_clamp_commit_reverify(&candidate, &verify,
                                              &committed, &reason));
    assert(committed == 0.0f);
    assert(reason == ECU_CLAMP_REASON_DEADLINE_OVERRUN);

    /* Contract age is wrap-safe. */
    candidate.computation_timestamp_us = UINT32_MAX - 5000u;
    verify.commit_timestamp_us = 4000u;
    assert(ecu_torque_clamp_commit_reverify(&candidate, &verify,
                                             &committed, &reason));
    assert(committed == candidate.selected_torque_nm);

    candidate.physical_zero_confirmation_required = true;
    verify.physical_zero_confirmed = false;
    assert(!ecu_torque_clamp_commit_reverify(&candidate, &verify,
                                              &committed, &reason));
    assert(committed == 0.0f);
    assert(reason == ECU_CLAMP_REASON_REVERSAL_WAIT);
    verify.physical_zero_confirmed = true;
    assert(ecu_torque_clamp_commit_reverify(&candidate, &verify,
                                             &committed, &reason));
    candidate.physical_zero_confirmation_required = false;

    /* Zero remains commandable even when authority/calibration are invalid. */
    candidate.selected_torque_nm = 0.0f;
    candidate.reason = ECU_CLAMP_REASON_ZERO_REQUEST;
    verify.authority_valid = false;
    verify.calibration_valid = false;
    verify.safety_gate_valid = false;
    assert(ecu_torque_clamp_commit_reverify(&candidate, &verify,
                                             &committed, &reason));
    assert(committed == 0.0f);
    assert(reason == ECU_CLAMP_REASON_ZERO_REQUEST);
}

static void test_zero_and_authority_state(void)
{
    ecu_pack_current_calibration_t cal = calibration();
    ecu_pack_current_calibration_runtime_t runtime = qualify(&cal);
    ecu_torque_clamp_state_t state = confirmed_zero_state();
    ecu_torque_clamp_input_t in = input(0.0f);
    in.discharge_authorized = false;
    in.charge_authorized = false;
    in.dcl_a = 0.1f;
    ecu_torque_clamp_output_t out;
    assert(ecu_torque_clamp_run(&in, &runtime, &state, &out));
    assert(out.selected_torque_nm == 0.0f);
    assert(out.battery_authority_state ==
           ECU_BATTERY_AUTHORITY_ZERO_STEADY_AUX_INFEASIBLE);
}

static void test_low_authority_uses_only_predicted_current_directions(void)
{
    ecu_pack_current_calibration_t cal = calibration();
    cal.numeric_margin_negative_a = 0.0f;
    cal.crc32 = ecu_pack_current_calibration_crc32(&cal);
    ecu_pack_current_calibration_runtime_t runtime = qualify(&cal);
    ecu_torque_clamp_state_t state = confirmed_zero_state();
    state.path_state = ECU_TORQUE_PATH_SAME_SIGN_TRACKING;
    state.raw_committed_torque_nm = 20.0f;
    state.normalized_zero = false;
    state.last_nonzero_committed_sign = ECU_TORQUE_SIGN_POSITIVE;
    state.transition.settled_anchor_torque_nm = 20.0f;
    state.transition.latest_raw_target_torque_nm = 20.0f;
    state.monitor_phase = ECU_CURRENT_MONITOR_STEADY;
    ecu_torque_clamp_input_t in = input(20.0f);
    in.dcl_a = 80.0f;
    in.ccl_a = 0.0f;
    in.charge_authorized = false;
    ecu_torque_clamp_output_t out;
    assert(ecu_torque_clamp_run(&in, &runtime, &state, &out));
    assert(out.selected_nonzero);
    assert(out.steady_current_a.min_a >= 0.0f);
    assert(out.battery_authority_state == ECU_BATTERY_AUTHORITY_NORMAL);
}

static void test_region_overflow_and_monotonic_validation(void)
{
    ecu_pack_current_calibration_t cal = calibration();
    /* A larger envelope that fails to contain the smaller one must be rejected. */
    for(uint16_t i = 0u; i < cal.transition_cell_count; ++i)
    {
        if((cal.transition_cells[i].profile ==
            ECU_TRANSITION_PROFILE_SLEW_LIMITED) &&
           (cal.transition_cells[i].direction ==
            ECU_TRANSITION_DIRECTION_FROM_ZERO) &&
           (cal.transition_cells[i].span_max_nm == 100.0f))
        {
            cal.transition_cells[i].regions[0].absolute_pack_current_a.max_a = 40.0f;
            break;
        }
    }
    cal.crc32 = ecu_pack_current_calibration_crc32(&cal);
    assert(!ecu_pack_current_calibration_validate_full(&cal));

    cal = calibration();
    cal.steady_cells[1].region_count = 2u;
    cal.steady_cells[1].regions[1] = cal.steady_cells[1].regions[0];
    cal.steady_cells[1].regions[0].motor_temp_max_c = 40.0f;
    cal.steady_cells[1].regions[1].motor_temp_min_c = 60.0f;
    cal.crc32 = ecu_pack_current_calibration_crc32(&cal);
    assert(!ecu_pack_current_calibration_validate_full(&cal));

    cal = calibration();
    cal.torque_axis_points = 6u;
    cal.steady_cell_count = 5u;
    cal.torque_axis_nm[5] = 250.0f;
    cal.steady_cells[4].torque_min_nm = 200.0f;
    cal.steady_cells[4].torque_max_nm = 250.0f;
    cal.steady_cells[4].region_count = 1u;
    cal.steady_cells[4].regions[0] = cal.steady_cells[3].regions[0];
    cal.steady_cells[4].regions[0].current_a =
        (ecu_current_interval_t){30.0f, 140.0f};
    for(uint16_t i = 0u; i < cal.transition_cell_count; ++i)
    {
        if(cal.transition_cells[i].span_max_nm == 200.0f)
        {
            cal.transition_cells[i].span_max_nm = 250.0f;
        }
    }
    cal.torque_uncertainty_negative_nm = 174.0f;
    cal.torque_uncertainty_positive_nm = 174.0f;
    cal.crc32 = ecu_pack_current_calibration_crc32(&cal);
    ecu_pack_current_calibration_runtime_t runtime = qualify(&cal);
    ecu_steady_current_input_t model_input = {
        .raw_torque_nm = 75.0f,
        .motor_speed_rpm = 3000.0f,
        .dc_bus_voltage_v = 300.0f,
        .inverter_temp_c = 30.0f,
        .motor_temp_c = 30.0f,
        .motor_speed_age_us = 0u,
        .dc_bus_voltage_age_us = 0u,
        .inverter_temp_age_us = 0u,
        .motor_temp_age_us = 0u,
    };
    ecu_steady_current_output_t model_output;
    assert(ecu_pack_current_evaluate_steady(&model_input, &runtime,
                                             &model_output) ==
           ECU_CURRENT_MODEL_REGION_OVERFLOW);
}


static void test_motor_temperature_domain_and_age(void)
{
    ecu_pack_current_calibration_t cal = calibration();
    ecu_steady_current_cell_t *cell = &cal.steady_cells[1];
    cell->region_count = 2u;
    cell->regions[1] = cell->regions[0];
    cell->regions[0].motor_temp_max_c = 50.0f;
    cell->regions[1].motor_temp_min_c = 50.0f;
    cell->regions[1].current_a = (ecu_current_interval_t){5.0f, 70.0f};
    cal.crc32 = ecu_pack_current_calibration_crc32(&cal);
    ecu_pack_current_calibration_runtime_t runtime = qualify(&cal);

    ecu_steady_current_input_t model_input = {
        .raw_torque_nm = 25.0f,
        .motor_speed_rpm = 3000.0f,
        .dc_bus_voltage_v = 300.0f,
        .inverter_temp_c = 30.0f,
        .motor_temp_c = 49.0f,
        .motor_speed_age_us = 1000u,
        .dc_bus_voltage_age_us = 1000u,
        .inverter_temp_age_us = 1000u,
        .motor_temp_age_us = 1000u,
    };
    ecu_steady_current_output_t steady;
    assert(ecu_pack_current_evaluate_steady(&model_input, &runtime, &steady) ==
           ECU_CURRENT_MODEL_OK);
    assert(steady.regions_evaluated == 2u);
    assert(steady.pack_current_a.max_a > 70.0f);

    model_input.motor_temp_age_us = cal.maximum_motor_temperature_age_us + 1u;
    assert(ecu_pack_current_evaluate_steady(&model_input, &runtime, &steady) ==
           ECU_CURRENT_MODEL_OUT_OF_DOMAIN);

    cal = calibration();
    for(uint16_t i = 0u; i < cal.transition_cell_count; ++i)
    {
        if((cal.transition_cells[i].profile ==
            ECU_TRANSITION_PROFILE_SLEW_LIMITED) &&
           (cal.transition_cells[i].direction ==
            ECU_TRANSITION_DIRECTION_FROM_ZERO))
        {
            cal.transition_cells[i].regions[0].motor_temp_max_c = 20.0f;
        }
    }
    cal.certified_motor_temp_max_c = 20.0f;
    cal.crc32 = ecu_pack_current_calibration_crc32(&cal);
    runtime = qualify(&cal);
    ecu_transition_current_input_t transition_input = {
        .settled_anchor_torque_nm = 0.0f,
        .minimum_raw_commanded_torque_nm = 0.0f,
        .maximum_raw_commanded_torque_nm = 20.0f,
        .latest_raw_target_torque_nm = 20.0f,
        .motor_speed_rpm = 3000.0f,
        .dc_bus_voltage_v = 300.0f,
        .inverter_temp_c = 10.0f,
        .motor_temp_c = 30.0f,
        .motor_speed_age_us = 1000u,
        .dc_bus_voltage_age_us = 1000u,
        .inverter_temp_age_us = 1000u,
        .motor_temp_age_us = 1000u,
        .profile = ECU_TRANSITION_PROFILE_SLEW_LIMITED,
        .direction = ECU_TRANSITION_DIRECTION_FROM_ZERO,
    };
    ecu_transition_current_output_t transition;
    assert(ecu_pack_current_evaluate_transition(
               &transition_input, &runtime, &transition) ==
           ECU_CURRENT_MODEL_OUT_OF_DOMAIN);
}


static void test_static_zero_deadline_fallback(void)
{
    ecu_pack_current_calibration_t cal = calibration();
    ecu_pack_current_calibration_runtime_t runtime = qualify(&cal);
    ecu_torque_clamp_state_t state = confirmed_zero_state();
    state.raw_committed_torque_nm = 40.0f;
    state.normalized_zero = false;
    ecu_torque_clamp_input_t in = input(80.0f);
    ecu_torque_clamp_output_t out;
    memset(&out, 0, sizeof(out));
    out.output_valid = true;
    out.steady_model_calls = 7u;
    ecu_torque_clamp_force_static_zero(
        &in, &runtime, &state, ECU_CLAMP_REASON_DEADLINE_OVERRUN, &out);
    assert(out.output_valid);
    assert(out.selected_torque_nm == 0.0f);
    assert(out.reason == ECU_CLAMP_REASON_DEADLINE_OVERRUN);
    assert(out.zero_fallback_valid);
    assert(out.material_change);
    assert(out.steady_model_calls == 7u);
    assert(out.computation_timestamp_us == in.now_us);
}

static uint32_t fuzz_state = 0x4D434C50u;

static uint32_t fuzz_next(void)
{
    fuzz_state = fuzz_state * 1664525u + 1013904223u;
    return fuzz_state;
}

static float fuzz_range(float minimum, float maximum)
{
    const float unit = (float)(fuzz_next() & 0x00FFFFFFu) /
                       (float)0x01000000u;
    return minimum + unit * (maximum - minimum);
}

static bool test_interval_authorized(ecu_current_interval_t interval,
                                     const ecu_torque_clamp_input_t *input)
{
    if((interval.min_a > interval.max_a) ||
       (interval.max_a > input->dcl_a) ||
       (interval.min_a < -input->ccl_a))
    {
        return false;
    }
    if((interval.max_a > 0.0f) && !input->discharge_authorized)
    {
        return false;
    }
    if((interval.min_a < 0.0f) && !input->charge_authorized)
    {
        return false;
    }
    return true;
}

static void test_randomized_contract_properties(void)
{
    ecu_pack_current_calibration_t cal = calibration();
    ecu_pack_current_calibration_runtime_t runtime = qualify(&cal);

    for(uint32_t iteration = 0u; iteration < 20000u; ++iteration)
    {
        ecu_torque_clamp_state_t state = confirmed_zero_state();
        ecu_torque_clamp_input_t in = input(fuzz_range(-130.0f, 230.0f));
        in.motor_speed_rpm = fuzz_range(-18000.0f, 18000.0f);
        in.dc_bus_voltage_v = fuzz_range(120.0f, 380.0f);
        in.inverter_temp_c = fuzz_range(-30.0f, 130.0f);
        in.motor_temp_c = fuzz_range(-30.0f, 130.0f);
        in.dcl_a = fuzz_range(0.0f, 130.0f);
        in.ccl_a = fuzz_range(0.0f, 60.0f);
        in.motor_speed_age_us = fuzz_next() % 100001u;
        in.dc_bus_voltage_age_us = fuzz_next() % 100001u;
        in.inverter_temp_age_us = fuzz_next() % 200001u;
        in.motor_temp_age_us = fuzz_next() % 200001u;
        in.discharge_authorized = (fuzz_next() & 1u) != 0u;
        in.charge_authorized = (fuzz_next() & 1u) != 0u;

        ecu_torque_clamp_output_t out;
        assert(ecu_torque_clamp_run(&in, &runtime, &state, &out));
        assert(out.output_valid);
        assert(isfinite(out.selected_torque_nm));
        assert(out.selected_torque_nm <= in.cm200_positive_cap_nm + 0.0001f);
        assert(out.selected_torque_nm >= in.cm200_negative_cap_nm - 0.0001f);
        assert(fabsf(out.selected_torque_nm * 10.0f -
                     roundf(out.selected_torque_nm * 10.0f)) < 0.001f);
        assert(out.steady_model_calls <= ECU_CLAMP_MAX_STEADY_MODEL_CALLS);
        assert(out.transition_model_calls <=
               ECU_CLAMP_MAX_TRANSITION_MODEL_CALLS);
        assert(out.torque_cells_evaluated <=
               ECU_CLAMP_MAX_TORQUE_CELL_EVALUATIONS);
        assert(out.transition_refinement_iterations <=
               ECU_CLAMP_MAX_TRANSITION_REFINE_ITERS);

        if(out.selected_nonzero)
        {
            assert(test_interval_authorized(out.steady_current_a, &in));
            if(out.transition_authority_required)
            {
                assert(out.transition_interval_valid);
                assert(test_interval_authorized(out.transition_current_a, &in));
            }
        }
        else
        {
            assert(out.selected_torque_nm == 0.0f);
        }
    }
}

int main(void)
{
    ecu_pack_current_calibration_t cal = calibration();
    ecu_pack_current_calibration_runtime_t runtime = qualify(&cal);
    ecu_torque_clamp_state_t state = confirmed_zero_state();
    ecu_torque_clamp_input_t in = input(80.07f);
    ecu_torque_clamp_output_t out;
    assert(ecu_torque_clamp_run(&in, &runtime, &state, &out));
    assert(out.output_valid);
    assert(fabsf(out.selected_torque_nm * 10.0f -
                 truncf(out.selected_torque_nm * 10.0f)) < 0.001f);
    assert(out.torque_cells_evaluated >= 2u);

    test_cell_aligned_path_cannot_skip_first_cell();
    test_increase_stops_inside_last_feasible_cell();
    test_transition_refinement();
    test_unknown_previous_uses_unknown_to_zero_profile();
    test_reversal_zero_gate();
    test_hold_with_unfinished_unauthorized_transition_goes_zero();
    test_transition_settles_and_reanchors();
    test_microstep_drift_guard();
    test_commit_reverify();
    test_zero_and_authority_state();
    test_low_authority_uses_only_predicted_current_directions();
    test_region_overflow_and_monotonic_validation();
    test_motor_temperature_domain_and_age();
    test_static_zero_deadline_fallback();
    test_randomized_contract_properties();

    puts("PASS torque clamp contract tests");
    return 0;
}
