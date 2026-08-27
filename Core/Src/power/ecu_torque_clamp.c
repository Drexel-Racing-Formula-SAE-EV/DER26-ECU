#include "power/ecu_torque_clamp.h"

#include <math.h>
#include <string.h>

#define ECU_CM200_TORQUE_COMMAND_QUANTUM_NM 0.1f

static float quantize_torque_toward_zero(float torque_nm)
{
    if(!isfinite(torque_nm))
    {
        return 0.0f;
    }
    return truncf(torque_nm / ECU_CM200_TORQUE_COMMAND_QUANTUM_NM) *
           ECU_CM200_TORQUE_COMMAND_QUANTUM_NM;
}

static ecu_torque_sign_t sign_of(float torque)
{
    return (torque > 0.0f) ? ECU_TORQUE_SIGN_POSITIVE :
           (torque < 0.0f) ? ECU_TORQUE_SIGN_NEGATIVE :
                            ECU_TORQUE_SIGN_NONE;
}

static bool interval_valid(ecu_current_interval_t interval)
{
    return isfinite(interval.min_a) && isfinite(interval.max_a) &&
           (interval.min_a <= interval.max_a);
}

static bool interval_authorized(ecu_current_interval_t interval,
                                float dcl_a,
                                float ccl_a,
                                bool discharge_authorized,
                                bool charge_authorized)
{
    if(!interval_valid(interval) || !isfinite(dcl_a) || !isfinite(ccl_a) ||
       (dcl_a < 0.0f) || (ccl_a < 0.0f))
    {
        return false;
    }
    if((interval.max_a > dcl_a) || (interval.min_a < -ccl_a))
    {
        return false;
    }
    if((interval.max_a > 0.0f) && !discharge_authorized)
    {
        return false;
    }
    if((interval.min_a < 0.0f) && !charge_authorized)
    {
        return false;
    }
    return true;
}

static float clamp_capability(float request, const ecu_torque_clamp_input_t *input)
{
    if(request > input->cm200_positive_cap_nm)
    {
        return input->cm200_positive_cap_nm;
    }
    if(request < input->cm200_negative_cap_nm)
    {
        return input->cm200_negative_cap_nm;
    }
    return request;
}

static bool normalized_zero(float raw,
                            bool was_zero,
                            const ecu_pack_current_calibration_t *cal)
{
    const float threshold = was_zero ? cal->zero_exit_nm : cal->zero_enter_nm;
    return fabsf(raw) <= threshold;
}

static ecu_steady_current_input_t steady_model_input(
    float torque_nm,
    bool microstep,
    const ecu_torque_clamp_input_t *input)
{
    return (ecu_steady_current_input_t){
        .raw_torque_nm = torque_nm,
        .motor_speed_rpm = input->motor_speed_rpm,
        .dc_bus_voltage_v = input->dc_bus_voltage_v,
        .inverter_temp_c = input->inverter_temp_c,
        .motor_temp_c = input->motor_temp_c,
        .motor_speed_age_us = input->motor_speed_age_us,
        .dc_bus_voltage_age_us = input->dc_bus_voltage_age_us,
        .inverter_temp_age_us = input->inverter_temp_age_us,
        .motor_temp_age_us = input->motor_temp_age_us,
        .apply_microstep_margin = microstep
    };
}

void ecu_torque_clamp_state_init(ecu_torque_clamp_state_t *state)
{
    if(state == NULL)
    {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->path_state = ECU_TORQUE_PATH_UNKNOWN;
    state->monitor_phase = ECU_CURRENT_MONITOR_STEP_TRANSITION;
    state->transition.profile = ECU_TRANSITION_PROFILE_INVALID;
    state->transition.direction = ECU_TRANSITION_DIRECTION_INVALID;
}

static bool evaluate_exact_steady(float torque_nm,
                                  bool microstep,
                                  const ecu_torque_clamp_input_t *input,
                                  const ecu_pack_current_calibration_runtime_t *runtime,
                                  ecu_torque_clamp_output_t *output,
                                  ecu_current_interval_t *interval)
{
    ecu_steady_current_input_t model_input =
        steady_model_input(torque_nm, microstep, input);
    ecu_steady_current_output_t model_output;
    output->steady_model_calls++;
    if(ecu_pack_current_evaluate_steady(&model_input, runtime, &model_output) !=
       ECU_CURRENT_MODEL_OK)
    {
        return false;
    }
    output->torque_cells_evaluated += model_output.torque_cells_evaluated;
    *interval = model_output.pack_current_a;
    return true;
}

static bool evaluate_exact_steady_authorized(
    float torque_nm,
    bool microstep,
    const ecu_torque_clamp_input_t *input,
    const ecu_pack_current_calibration_runtime_t *runtime,
    ecu_torque_clamp_output_t *output,
    ecu_current_interval_t *interval)
{
    return evaluate_exact_steady(torque_nm, microstep, input, runtime,
                                 output, interval) &&
           interval_authorized(*interval, input->dcl_a, input->ccl_a,
                               input->discharge_authorized,
                               input->charge_authorized);
}

static bool evaluate_cell_authorized(
    uint16_t cell_index,
    const ecu_torque_clamp_input_t *input,
    const ecu_pack_current_calibration_runtime_t *runtime,
    ecu_torque_clamp_output_t *output,
    ecu_current_interval_t *interval)
{
    ecu_steady_current_input_t model_input =
        steady_model_input(0.0f, false, input);
    ecu_steady_current_output_t model_output;
    output->steady_model_calls++;
    output->torque_cells_evaluated++;
    if(ecu_pack_current_evaluate_steady_cell(cell_index, &model_input, runtime,
                                             &model_output) !=
       ECU_CURRENT_MODEL_OK)
    {
        return false;
    }
    *interval = model_output.pack_current_a;
    return interval_authorized(*interval, input->dcl_a, input->ccl_a,
                               input->discharge_authorized,
                               input->charge_authorized);
}

static bool cell_path_overlap(float path_min,
                              float path_max,
                              float cell_min,
                              float cell_max)
{
    const float overlap_min = fmaxf(path_min, cell_min);
    const float overlap_max = fminf(path_max, cell_max);
    return overlap_max > overlap_min;
}

static float positive_cell_interior_candidate(
    const ecu_steady_current_cell_t *cell,
    const ecu_pack_current_calibration_t *cal,
    float maximum);
static float negative_cell_interior_candidate(
    const ecu_steady_current_cell_t *cell,
    const ecu_pack_current_calibration_t *cal,
    float minimum);

static bool select_increase_prefix(
    float previous,
    float request,
    const ecu_torque_clamp_input_t *input,
    const ecu_pack_current_calibration_runtime_t *runtime,
    ecu_torque_clamp_output_t *output,
    float *selected,
    ecu_current_interval_t *selected_interval)
{
    const ecu_pack_current_calibration_t *cal = runtime->calibration;
    const bool upward = request > previous;
    const float path_min = fminf(previous, request);
    const float path_max = fmaxf(previous, request);
    bool touched = false;
    bool accepted = false;
    float best = previous;
    ecu_current_interval_t best_interval = {0.0f, 0.0f};

    if(upward)
    {
        for(uint16_t index = 0u; index < cal->steady_cell_count; ++index)
        {
            const ecu_steady_current_cell_t *cell = &cal->steady_cells[index];
            if(!cell_path_overlap(path_min, path_max,
                                  cell->torque_min_nm, cell->torque_max_nm))
            {
                continue;
            }
            touched = true;
            ecu_current_interval_t interval;
            if(!evaluate_cell_authorized(index, input, runtime, output, &interval))
            {
                break;
            }
            accepted = true;
            best_interval = interval;
            best = quantize_torque_toward_zero(
                positive_cell_interior_candidate(
                    cell, cal, fminf(request, cell->torque_max_nm)));
        }
    }
    else
    {
        for(uint16_t reverse = cal->steady_cell_count; reverse > 0u; --reverse)
        {
            const uint16_t index = (uint16_t)(reverse - 1u);
            const ecu_steady_current_cell_t *cell = &cal->steady_cells[index];
            if(!cell_path_overlap(path_min, path_max,
                                  cell->torque_min_nm, cell->torque_max_nm))
            {
                continue;
            }
            touched = true;
            ecu_current_interval_t interval;
            if(!evaluate_cell_authorized(index, input, runtime, output, &interval))
            {
                break;
            }
            accepted = true;
            best_interval = interval;
            best = quantize_torque_toward_zero(
                negative_cell_interior_candidate(
                    cell, cal, fmaxf(request, cell->torque_min_nm)));
        }
    }

    if(!touched || !accepted)
    {
        *selected = previous;
        return false;
    }

    *selected = best;
    *selected_interval = best_interval;
    return best != previous;
}

static float positive_cell_interior_candidate(
    const ecu_steady_current_cell_t *cell,
    const ecu_pack_current_calibration_t *cal,
    float maximum)
{
    const float boundary_guard_nm = fmaxf(
        cal->torque_uncertainty_positive_nm,
        ECU_CM200_TORQUE_COMMAND_QUANTUM_NM);
    float candidate = fminf(maximum,
                            cell->torque_max_nm - boundary_guard_nm);
    const float minimum = cell->torque_min_nm +
                          cal->torque_uncertainty_negative_nm;
    if(candidate < minimum)
    {
        candidate = 0.5f * (cell->torque_min_nm + cell->torque_max_nm);
    }
    return candidate;
}

static float negative_cell_interior_candidate(
    const ecu_steady_current_cell_t *cell,
    const ecu_pack_current_calibration_t *cal,
    float minimum)
{
    const float boundary_guard_nm = fmaxf(
        cal->torque_uncertainty_negative_nm,
        ECU_CM200_TORQUE_COMMAND_QUANTUM_NM);
    float candidate = fmaxf(minimum,
                            cell->torque_min_nm + boundary_guard_nm);
    const float maximum = cell->torque_max_nm -
                          cal->torque_uncertainty_positive_nm;
    if(candidate > maximum)
    {
        candidate = 0.5f * (cell->torque_min_nm + cell->torque_max_nm);
    }
    return candidate;
}

static bool select_reduction_toward_zero(
    float request,
    const ecu_torque_clamp_input_t *input,
    const ecu_pack_current_calibration_runtime_t *runtime,
    ecu_torque_clamp_output_t *output,
    float *selected,
    ecu_current_interval_t *selected_interval)
{
    const ecu_pack_current_calibration_t *cal = runtime->calibration;
    if(request > 0.0f)
    {
        for(uint16_t reverse = cal->steady_cell_count; reverse > 0u; --reverse)
        {
            const uint16_t index = (uint16_t)(reverse - 1u);
            const ecu_steady_current_cell_t *cell = &cal->steady_cells[index];
            if((cell->torque_max_nm <= 0.0f) ||
               (cell->torque_min_nm >= request))
            {
                continue;
            }
            ecu_current_interval_t interval;
            if(!evaluate_cell_authorized(index, input, runtime, output, &interval))
            {
                continue;
            }
            const float candidate = quantize_torque_toward_zero(
                positive_cell_interior_candidate(cell, cal, request));
            if(normalized_zero(candidate, false, cal))
            {
                break;
            }
            ecu_current_interval_t exact;
            if(evaluate_exact_steady_authorized(candidate, false, input, runtime,
                                                 output, &exact))
            {
                *selected = candidate;
                *selected_interval = exact;
                return true;
            }
        }
    }
    else if(request < 0.0f)
    {
        for(uint16_t index = 0u; index < cal->steady_cell_count; ++index)
        {
            const ecu_steady_current_cell_t *cell = &cal->steady_cells[index];
            if((cell->torque_min_nm >= 0.0f) ||
               (cell->torque_max_nm <= request))
            {
                continue;
            }
            ecu_current_interval_t interval;
            if(!evaluate_cell_authorized(index, input, runtime, output, &interval))
            {
                continue;
            }
            const float candidate = quantize_torque_toward_zero(
                negative_cell_interior_candidate(cell, cal, request));
            if(normalized_zero(candidate, false, cal))
            {
                break;
            }
            ecu_current_interval_t exact;
            if(evaluate_exact_steady_authorized(candidate, false, input, runtime,
                                                 output, &exact))
            {
                *selected = candidate;
                *selected_interval = exact;
                return true;
            }
        }
    }

    *selected = 0.0f;
    *selected_interval = (ecu_current_interval_t){0.0f, 0.0f};
    return false;
}

static void movement_metadata(const ecu_torque_clamp_state_t *state,
                              float previous,
                              float candidate,
                              bool reversal_first_leg,
                              ecu_transition_profile_t *profile,
                              ecu_transition_direction_t *direction)
{
    if(candidate == 0.0f)
    {
        *profile = reversal_first_leg ?
            ECU_TRANSITION_PROFILE_REVERSAL_TO_ZERO :
            ECU_TRANSITION_PROFILE_ZERO_ASSERT;
        *direction = reversal_first_leg ?
            ECU_TRANSITION_DIRECTION_REVERSAL_FIRST_LEG :
            ECU_TRANSITION_DIRECTION_TO_ZERO;
        return;
    }

    if(state->transition.active && (candidate == previous))
    {
        *profile = state->transition.profile;
        *direction = state->transition.direction;
        return;
    }

    if(previous == 0.0f)
    {
        *profile = state->transition.active ?
            ECU_TRANSITION_PROFILE_COMPOSED :
            ECU_TRANSITION_PROFILE_SLEW_LIMITED;
        *direction = ECU_TRANSITION_DIRECTION_FROM_ZERO;
        return;
    }

    *profile = state->transition.active ?
        ECU_TRANSITION_PROFILE_COMPOSED :
        ECU_TRANSITION_PROFILE_SLEW_LIMITED;
    *direction = (fabsf(candidate) > fabsf(previous)) ?
        ECU_TRANSITION_DIRECTION_SAME_SIGN_INCREASE :
        ECU_TRANSITION_DIRECTION_SAME_SIGN_DECREASE;
}

static bool evaluate_transition_for_candidate(
    float candidate,
    ecu_transition_profile_t profile,
    ecu_transition_direction_t direction,
    const ecu_torque_clamp_input_t *input,
    const ecu_pack_current_calibration_runtime_t *runtime,
    const ecu_torque_clamp_state_t *state,
    ecu_torque_clamp_output_t *output,
    ecu_current_interval_t *interval,
    uint32_t *maximum_settling_time_us)
{
    const float previous = state->raw_committed_torque_nm;
    /* Even when no physical transition is currently active, the settled
     * anchor may differ from the previous command because certified
     * microsteps are allowed inside the tracking band. A drift-triggered
     * material transition must therefore cover the full anchor-to-candidate
     * bracket, not only the latest 10 ms delta. */
    const float anchor = state->transition.settled_anchor_torque_nm;
    const float inactive_minimum = fminf(anchor, fminf(previous, candidate));
    const float inactive_maximum = fmaxf(anchor, fmaxf(previous, candidate));
    const float minimum = state->transition.active ?
        fminf(state->transition.minimum_raw_commanded_torque_nm, candidate) :
        inactive_minimum;
    const float maximum = state->transition.active ?
        fmaxf(state->transition.maximum_raw_commanded_torque_nm, candidate) :
        inactive_maximum;

    ecu_transition_current_input_t model_input = {
        .settled_anchor_torque_nm = anchor,
        .minimum_raw_commanded_torque_nm = minimum,
        .maximum_raw_commanded_torque_nm = maximum,
        .latest_raw_target_torque_nm = candidate,
        .motor_speed_rpm = input->motor_speed_rpm,
        .dc_bus_voltage_v = input->dc_bus_voltage_v,
        .inverter_temp_c = input->inverter_temp_c,
        .motor_temp_c = input->motor_temp_c,
        .motor_speed_age_us = input->motor_speed_age_us,
        .dc_bus_voltage_age_us = input->dc_bus_voltage_age_us,
        .inverter_temp_age_us = input->inverter_temp_age_us,
        .motor_temp_age_us = input->motor_temp_age_us,
        .profile = profile,
        .direction = direction
    };
    ecu_transition_current_output_t model_output;
    output->transition_model_calls++;
    if(ecu_pack_current_evaluate_transition(&model_input, runtime,
                                            &model_output) !=
       ECU_CURRENT_MODEL_OK)
    {
        return false;
    }
    *interval = model_output.absolute_pack_current_a;
    *maximum_settling_time_us = model_output.maximum_settling_time_us;
    return true;
}

static void zero_transition_metadata(
    const ecu_torque_clamp_input_t *input,
    const ecu_torque_clamp_state_t *state,
    bool reversal_first_leg,
    ecu_transition_profile_t *profile,
    ecu_transition_direction_t *direction)
{
    if(reversal_first_leg)
    {
        *profile = ECU_TRANSITION_PROFILE_REVERSAL_TO_ZERO;
        *direction = ECU_TRANSITION_DIRECTION_REVERSAL_FIRST_LEG;
    }
    else if((state->path_state == ECU_TORQUE_PATH_UNKNOWN) &&
            !input->physical_zero_confirmed)
    {
        *profile = ECU_TRANSITION_PROFILE_UNKNOWN_TO_ZERO;
        *direction = ECU_TRANSITION_DIRECTION_TO_ZERO;
    }
    else
    {
        *profile = ECU_TRANSITION_PROFILE_ZERO_ASSERT;
        *direction = ECU_TRANSITION_DIRECTION_TO_ZERO;
    }
}

static void evaluate_zero_fallback(
    const ecu_torque_clamp_input_t *input,
    const ecu_pack_current_calibration_runtime_t *runtime,
    const ecu_torque_clamp_state_t *state,
    bool reversal_first_leg,
    ecu_torque_clamp_output_t *output)
{
    const ecu_pack_current_calibration_t *cal = runtime->calibration;
    ecu_transition_profile_t profile;
    ecu_transition_direction_t direction;
    zero_transition_metadata(input, state, reversal_first_leg,
                             &profile, &direction);
    ecu_current_interval_t interval;
    uint32_t settling_time_us = 0u;
    if(evaluate_transition_for_candidate(
           0.0f, profile, direction,
           input, runtime, state, output, &interval, &settling_time_us))
    {
        output->zero_fallback_transition_current_a = interval;
        output->zero_fallback_maximum_settling_time_us = settling_time_us;
        output->zero_fallback_valid = true;
    }
    else
    {
        output->zero_fallback_transition_current_a =
            cal->late_zero_transition_current_a;
        output->zero_fallback_maximum_settling_time_us =
            cal->late_zero_maximum_settling_time_us;
        output->zero_fallback_valid =
            interval_valid(cal->late_zero_transition_current_a);
    }
}

static bool command_rate_allows_microstep(
    float delta_nm,
    const ecu_torque_clamp_input_t *input,
    const ecu_torque_clamp_state_t *state,
    const ecu_pack_current_calibration_t *cal)
{
    if(state->transition.last_commit_us == 0u)
    {
        return false;
    }
    const uint32_t elapsed_us = input->now_us - state->transition.last_commit_us;
    if(elapsed_us == 0u)
    {
        return false;
    }
    const float rate_nm_per_s = fabsf(delta_nm) * 1000000.0f /
                                (float)elapsed_us;
    return isfinite(rate_nm_per_s) &&
           (rate_nm_per_s <= cal->maximum_settled_command_rate_nm_per_s);
}

static bool microstep_eligible(float previous,
                               float request,
                               const ecu_torque_clamp_input_t *input,
                               const ecu_torque_clamp_state_t *state,
                               const ecu_pack_current_calibration_t *cal)
{
    const float delta = request - previous;
    const ecu_torque_sign_t previous_sign = sign_of(previous);
    const ecu_torque_sign_t request_sign = sign_of(request);
    return !state->transition.active &&
           (state->path_state == ECU_TORQUE_PATH_SAME_SIGN_TRACKING) &&
           (previous_sign != ECU_TORQUE_SIGN_NONE) &&
           (previous_sign == request_sign) &&
           !normalized_zero(previous, state->normalized_zero, cal) &&
           !normalized_zero(request, state->normalized_zero, cal) &&
           (fabsf(delta) > 0.0f) &&
           (fabsf(delta) <= cal->maximum_microstep_nm) &&
           (fabsf(delta) <= cal->tracking_band_nm) &&
           command_rate_allows_microstep(delta, input, state, cal) &&
           (fabsf(request - state->transition.settled_anchor_torque_nm) <=
            cal->maximum_anchor_deviation_nm) &&
           ((state->transition.cumulative_raw_drift_nm + fabsf(delta)) <=
            cal->maximum_cumulative_drift_nm);
}

static ecu_battery_authority_state_t classify_battery_authority(
    float selected_torque_nm,
    ecu_current_interval_t steady_interval,
    const ecu_torque_clamp_input_t *input,
    const ecu_pack_current_calibration_t *cal,
    bool nonzero_exhausted)
{
    if(selected_torque_nm == 0.0f)
    {
        if(interval_valid(steady_interval) &&
           ((steady_interval.max_a > input->dcl_a) ||
            (steady_interval.min_a < -input->ccl_a)))
        {
            return ECU_BATTERY_AUTHORITY_ZERO_STEADY_AUX_INFEASIBLE;
        }
        return nonzero_exhausted ? ECU_BATTERY_AUTHORITY_TORQUE_EXHAUSTED :
                                   ECU_BATTERY_AUTHORITY_NORMAL;
    }

    const bool discharge_direction_used = steady_interval.max_a > 0.0f;
    const bool charge_direction_used = steady_interval.min_a < 0.0f;
    const float discharge_headroom = input->dcl_a - steady_interval.max_a;
    const float charge_headroom = input->ccl_a + steady_interval.min_a;
    float consumption = 0.0f;
    if(discharge_direction_used && (input->dcl_a > 0.0f))
    {
        consumption = fmaxf(consumption,
                            steady_interval.max_a / input->dcl_a);
    }
    if(charge_direction_used && (input->ccl_a > 0.0f))
    {
        consumption = fmaxf(consumption,
                            -steady_interval.min_a / input->ccl_a);
    }

    if((consumption >= cal->low_margin_consumption_fraction) ||
       (discharge_direction_used &&
        (discharge_headroom < cal->minimum_discharge_headroom_a)) ||
       (charge_direction_used &&
        (charge_headroom < cal->minimum_charge_headroom_a)) ||
       (fabsf(selected_torque_nm) < cal->nonzero_torque_threshold_nm))
    {
        return ECU_BATTERY_AUTHORITY_LOW;
    }
    return ECU_BATTERY_AUTHORITY_NORMAL;
}

static bool execution_counts_within_contract(
    const ecu_torque_clamp_output_t *output)
{
    return (output->steady_model_calls <= ECU_CLAMP_MAX_STEADY_MODEL_CALLS) &&
           (output->transition_model_calls <=
            ECU_CLAMP_MAX_TRANSITION_MODEL_CALLS) &&
           (output->torque_cells_evaluated <=
            ECU_CLAMP_MAX_TORQUE_CELL_EVALUATIONS) &&
           (output->transition_refinement_iterations <=
            ECU_CLAMP_MAX_TRANSITION_REFINE_ITERS);
}

static void set_snapshot_generations(const ecu_torque_clamp_input_t *input,
                                     ecu_torque_clamp_output_t *output);

void ecu_torque_clamp_force_static_zero(
    const ecu_torque_clamp_input_t *input,
    const ecu_pack_current_calibration_runtime_t *runtime,
    const ecu_torque_clamp_state_t *state,
    ecu_torque_clamp_reason_t reason,
    ecu_torque_clamp_output_t *output)
{
    if((input == NULL) || (state == NULL) || (output == NULL))
    {
        return;
    }

    const bool preserve_counts = output->output_valid;
    const uint16_t steady_calls = preserve_counts ?
        output->steady_model_calls : 0u;
    const uint16_t transition_calls = preserve_counts ?
        output->transition_model_calls : 0u;
    const uint16_t cells = preserve_counts ?
        output->torque_cells_evaluated : 0u;
    const uint16_t boundary_iters = preserve_counts ?
        output->boundary_refinement_iterations : 0u;
    const uint16_t transition_iters = preserve_counts ?
        output->transition_refinement_iterations : 0u;
    memset(output, 0, sizeof(*output));
    output->steady_model_calls = steady_calls;
    output->transition_model_calls = transition_calls;
    output->torque_cells_evaluated = cells;
    output->boundary_refinement_iterations = boundary_iters;
    output->transition_refinement_iterations = transition_iters;
    output->selected_torque_nm = 0.0f;
    output->reason = reason;
    output->battery_authority_state = ECU_BATTERY_AUTHORITY_TORQUE_EXHAUSTED;
    output->transition_profile = ECU_TRANSITION_PROFILE_ZERO_ASSERT;
    output->transition_direction = ECU_TRANSITION_DIRECTION_TO_ZERO;
    output->material_change = state->raw_committed_torque_nm != 0.0f;
    output->selected_nonzero = false;
    output->output_valid = true;

    if(ecu_pack_current_calibration_runtime_valid(runtime))
    {
        const ecu_pack_current_calibration_t *cal = runtime->calibration;
        output->steady_current_a = cal->late_zero_transition_current_a;
        output->transition_current_a = cal->late_zero_transition_current_a;
        output->zero_fallback_transition_current_a =
            cal->late_zero_transition_current_a;
        output->transition_interval_valid = true;
        output->zero_fallback_valid = true;
        output->transition_maximum_settling_time_us =
            cal->late_zero_maximum_settling_time_us;
        output->zero_fallback_maximum_settling_time_us =
            cal->late_zero_maximum_settling_time_us;
        output->battery_authority_state =
            ECU_BATTERY_AUTHORITY_TORQUE_EXHAUSTED;
    }

    set_snapshot_generations(input, output);
}

static void set_snapshot_generations(const ecu_torque_clamp_input_t *input,
                                     ecu_torque_clamp_output_t *output)
{
    output->speed_generation = input->speed_generation;
    output->vdc_generation = input->vdc_generation;
    output->temperature_generation = input->temperature_generation;
    output->capability_generation = input->capability_generation;
    output->calibration_generation = input->calibration_generation;
    output->computation_timestamp_us = input->now_us;
}

static void finish_zero_output(
    const ecu_torque_clamp_input_t *input,
    const ecu_pack_current_calibration_runtime_t *runtime,
    const ecu_torque_clamp_state_t *state,
    bool reversal_first_leg,
    bool nonzero_exhausted,
    ecu_torque_clamp_reason_t reason,
    ecu_torque_clamp_output_t *output)
{
    ecu_current_interval_t zero_steady = {0.0f, 0.0f};
    const bool zero_steady_valid = evaluate_exact_steady(
        0.0f, false, input, runtime, output, &zero_steady);
    evaluate_zero_fallback(input, runtime, state, reversal_first_leg, output);
    if(!zero_steady_valid && output->zero_fallback_valid)
    {
        /* Zero remains commandable even with an invalid operating point. Use
         * the pre-certified absolute zero/decay envelope rather than claiming
         * an unqualified [0,0] prediction. */
        zero_steady = output->zero_fallback_transition_current_a;
    }
    output->selected_torque_nm = 0.0f;
    output->steady_current_a = zero_steady;
    output->transition_current_a = output->zero_fallback_transition_current_a;
    output->transition_interval_valid = output->zero_fallback_valid;
    output->transition_authority_required = false;
    zero_transition_metadata(input, state, reversal_first_leg,
                             &output->transition_profile,
                             &output->transition_direction);
    output->transition_maximum_settling_time_us =
        output->zero_fallback_maximum_settling_time_us;
    output->material_change = (state->raw_committed_torque_nm != 0.0f);
    output->selected_nonzero = false;
    output->reason = reason;
    output->battery_authority_state = zero_steady_valid ?
        classify_battery_authority(0.0f, zero_steady, input,
                                   runtime->calibration, nonzero_exhausted) :
        ECU_BATTERY_AUTHORITY_TORQUE_EXHAUSTED;
    output->output_valid = true;
    set_snapshot_generations(input, output);
}

bool ecu_torque_clamp_run(
    const ecu_torque_clamp_input_t *input,
    const ecu_pack_current_calibration_runtime_t *runtime,
    const ecu_torque_clamp_state_t *state,
    ecu_torque_clamp_output_t *output)
{
    if(output != NULL)
    {
        memset(output, 0, sizeof(*output));
    }
    if((input == NULL) || (state == NULL) || (output == NULL) ||
       !isfinite(input->requested_torque_nm) ||
       !isfinite(input->cm200_positive_cap_nm) ||
       !isfinite(input->cm200_negative_cap_nm) ||
       (input->cm200_positive_cap_nm < 0.0f) ||
       (input->cm200_negative_cap_nm > 0.0f))
    {
        if(output != NULL)
        {
            output->reason = ECU_CLAMP_REASON_INPUT_INVALID;
        }
        return false;
    }
    if(!ecu_pack_current_calibration_runtime_valid(runtime))
    {
        output->reason = ECU_CLAMP_REASON_CALIBRATION_INVALID;
        output->battery_authority_state =
            ECU_BATTERY_AUTHORITY_TORQUE_EXHAUSTED;
        output->output_valid = true;
        set_snapshot_generations(input, output);
        return true;
    }
    if(!input->operating_point_valid)
    {
        finish_zero_output(input, runtime, state, false, false,
                           ECU_CLAMP_REASON_INPUT_INVALID, output);
        return true;
    }
    if(!input->authority_valid)
    {
        finish_zero_output(input, runtime, state, false, false,
                           ECU_CLAMP_REASON_AUTHORITY_INVALID, output);
        return true;
    }

    const ecu_pack_current_calibration_t *cal = runtime->calibration;
    const bool effective_confirmed_zero = input->physical_zero_confirmed;
    if((state->path_state == ECU_TORQUE_PATH_UNKNOWN) &&
       !effective_confirmed_zero)
    {
        finish_zero_output(input, runtime, state, false, false,
                           ECU_CLAMP_REASON_REVERSAL_WAIT, output);
        return true;
    }

    const float previous = (state->path_state == ECU_TORQUE_PATH_UNKNOWN) ?
        0.0f : state->raw_committed_torque_nm;
    const bool previous_normalized_zero =
        (state->path_state == ECU_TORQUE_PATH_UNKNOWN) ? true :
                                                        state->normalized_zero;
    float request = quantize_torque_toward_zero(
        clamp_capability(input->requested_torque_nm, input));
    if(normalized_zero(request, previous_normalized_zero, cal))
    {
        finish_zero_output(input, runtime, state, false, false,
                           ECU_CLAMP_REASON_ZERO_REQUEST, output);
        return true;
    }

    const ecu_torque_sign_t request_sign = sign_of(request);
    const bool reversal_request =
        (state->last_nonzero_committed_sign != ECU_TORQUE_SIGN_NONE) &&
        (request_sign != state->last_nonzero_committed_sign);
    const bool physical_zero_confirmation_required =
        (state->path_state == ECU_TORQUE_PATH_UNKNOWN) || reversal_request;
    if(reversal_request && !effective_confirmed_zero)
    {
        finish_zero_output(input, runtime, state, true, false,
                           ECU_CLAMP_REASON_REVERSAL_WAIT, output);
        return true;
    }

    ecu_current_interval_t held_steady = {0.0f, 0.0f};
    bool held_steady_feasible = (previous == 0.0f) ? true :
        evaluate_exact_steady_authorized(previous, false, input, runtime,
                                         output, &held_steady);
    bool held_transition_feasible = true;
    ecu_current_interval_t held_transition = {0.0f, 0.0f};
    uint32_t held_settling_time_us = 0u;
    if((previous != 0.0f) && state->transition.active)
    {
        held_transition_feasible = evaluate_transition_for_candidate(
            previous,
            state->transition.profile,
            state->transition.direction,
            input, runtime, state, output, &held_transition,
            &held_settling_time_us) &&
            interval_authorized(held_transition, input->dcl_a, input->ccl_a,
                                input->discharge_authorized,
                                input->charge_authorized);
    }
    const bool held_feasible = held_steady_feasible && held_transition_feasible;

    const bool same_sign = (previous == 0.0f) ||
                           (sign_of(previous) == request_sign);
    const bool tracking_microstep = same_sign &&
        microstep_eligible(previous, request, input, state, cal);
    const bool increasing_magnitude = fabsf(request) > fabsf(previous);

    float selected = previous;
    ecu_current_interval_t selected_steady = held_steady;
    if(!same_sign)
    {
        finish_zero_output(input, runtime, state, true, false,
                           ECU_CLAMP_REASON_REVERSAL_WAIT, output);
        return true;
    }

    const bool hold_request = request == previous;
    if(!increasing_magnitude)
    {
        if(hold_request && held_feasible)
        {
            selected = previous;
            selected_steady = held_steady;
        }
        else if(hold_request && held_steady_feasible &&
                !held_transition_feasible)
        {
            /* The unfinished physical transition is no longer authorized.
             * Holding the numerical torque would preserve the exposure, so
             * take the always-available immediate reduction to zero. */
            finish_zero_output(input, runtime, state, false, true,
                               ECU_CLAMP_REASON_TRANSITION_LIMIT, output);
            return true;
        }
        else if(!hold_request &&
                evaluate_exact_steady_authorized(request, tracking_microstep,
                                                 input, runtime, output,
                                                 &selected_steady))
        {
            selected = request;
        }
        else if(!select_reduction_toward_zero(
                    hold_request ? previous : request,
                    input, runtime, output, &selected, &selected_steady))
        {
            finish_zero_output(input, runtime, state, false, true,
                               ECU_CLAMP_REASON_SEARCH_EXHAUSTED, output);
            return true;
        }
    }
    else
    {
        if(!held_feasible)
        {
            if(!select_reduction_toward_zero(previous, input, runtime, output,
                                              &selected, &selected_steady))
            {
                finish_zero_output(input, runtime, state, false, true,
                                   ECU_CLAMP_REASON_CURRENT_LIMIT, output);
                return true;
            }
        }
        else if(!select_increase_prefix(previous, request, input, runtime, output,
                                        &selected, &selected_steady))
        {
            selected = previous;
            selected_steady = held_steady;
            output->reason = ECU_CLAMP_REASON_CURRENT_LIMIT;
        }
    }

    const bool selected_microstep = tracking_microstep && (selected == request);
    ecu_transition_profile_t profile = ECU_TRANSITION_PROFILE_HOLD;
    ecu_transition_direction_t direction = ECU_TRANSITION_DIRECTION_HOLD;
    movement_metadata(state, previous, selected, false, &profile, &direction);

    ecu_current_interval_t selected_transition = {0.0f, 0.0f};
    uint32_t selected_settling_time_us = 0u;
    bool transition_valid = false;
    bool transition_authority_required = false;

    if(selected != 0.0f)
    {
        if(selected_microstep)
        {
            profile = ECU_TRANSITION_PROFILE_MICROSTEP;
            direction = (fabsf(selected) >= fabsf(previous)) ?
                ECU_TRANSITION_DIRECTION_SAME_SIGN_INCREASE :
                ECU_TRANSITION_DIRECTION_SAME_SIGN_DECREASE;
        }
        else if((selected != previous) || state->transition.active)
        {
            transition_valid = evaluate_transition_for_candidate(
                selected, profile, direction, input, runtime, state, output,
                &selected_transition, &selected_settling_time_us);
            transition_authority_required =
                (fabsf(selected) > fabsf(previous)) ||
                (selected == previous && state->transition.active);

            if(increasing_magnitude && (selected != previous) &&
               (!transition_valid ||
                !interval_authorized(selected_transition,
                                     input->dcl_a, input->ccl_a,
                                     input->discharge_authorized,
                                     input->charge_authorized)))
            {
                float safe = previous;
                float unsafe = selected;
                ecu_current_interval_t safe_steady = held_steady;
                ecu_current_interval_t safe_transition = held_transition;
                uint32_t safe_settling = held_settling_time_us;
                bool safe_transition_valid = state->transition.active ?
                    held_transition_feasible : true;

                for(uint8_t iteration = 0u;
                    iteration < ECU_CLAMP_MAX_TRANSITION_REFINE_ITERS;
                    ++iteration)
                {
                    const float midpoint = quantize_torque_toward_zero(
                        safe + 0.5f * (unsafe - safe));
                    ecu_current_interval_t midpoint_steady;
                    ecu_current_interval_t midpoint_transition;
                    uint32_t midpoint_settling = 0u;
                    output->transition_refinement_iterations++;
                    const bool midpoint_steady_ok =
                        evaluate_exact_steady_authorized(
                            midpoint, false, input, runtime, output,
                            &midpoint_steady);
                    const bool midpoint_transition_ok = midpoint_steady_ok &&
                        evaluate_transition_for_candidate(
                            midpoint, profile, direction, input, runtime, state,
                            output, &midpoint_transition,
                            &midpoint_settling) &&
                        interval_authorized(midpoint_transition,
                                            input->dcl_a, input->ccl_a,
                                            input->discharge_authorized,
                                            input->charge_authorized);
                    if(midpoint_transition_ok)
                    {
                        safe = midpoint;
                        safe_steady = midpoint_steady;
                        safe_transition = midpoint_transition;
                        safe_settling = midpoint_settling;
                        safe_transition_valid = true;
                    }
                    else
                    {
                        unsafe = midpoint;
                    }
                }

                if((safe != previous) && safe_transition_valid)
                {
                    selected = safe;
                    selected_steady = safe_steady;
                    selected_transition = safe_transition;
                    selected_settling_time_us = safe_settling;
                    transition_valid = true;
                }
                else if(held_feasible)
                {
                    selected = previous;
                    selected_steady = held_steady;
                    selected_transition = held_transition;
                    selected_settling_time_us = held_settling_time_us;
                    transition_valid = state->transition.active;
                    transition_authority_required = state->transition.active;
                    profile = state->transition.active ?
                        state->transition.profile : ECU_TRANSITION_PROFILE_HOLD;
                    direction = state->transition.active ?
                        state->transition.direction : ECU_TRANSITION_DIRECTION_HOLD;
                    output->reason = ECU_CLAMP_REASON_TRANSITION_LIMIT;
                }
                else
                {
                    finish_zero_output(input, runtime, state, false, true,
                                       ECU_CLAMP_REASON_TRANSITION_LIMIT, output);
                    return true;
                }
            }
            else if(!transition_valid && !increasing_magnitude)
            {
                /* A reduction is never held back by transient exposure. If its
                 * calibrated decay envelope is unavailable, go all the way to
                 * zero rather than retaining the previous nonzero torque. */
                finish_zero_output(input, runtime, state, false, true,
                                   ECU_CLAMP_REASON_TRANSITION_LIMIT, output);
                return true;
            }
        }
    }

    if((selected != 0.0f) &&
       !evaluate_exact_steady_authorized(selected, selected_microstep, input,
                                         runtime, output, &selected_steady))
    {
        finish_zero_output(input, runtime, state, false, true,
                           ECU_CLAMP_REASON_CURRENT_LIMIT, output);
        return true;
    }

    evaluate_zero_fallback(input, runtime, state, false, output);
    output->selected_torque_nm = selected;
    output->steady_current_a = selected_steady;
    output->transition_current_a = selected_transition;
    output->transition_interval_valid = transition_valid;
    output->transition_authority_required = transition_authority_required;
    output->transition_profile = profile;
    output->transition_direction = direction;
    output->transition_maximum_settling_time_us = selected_settling_time_us;
    output->microstep_applied = selected_microstep;
    output->material_change = !selected_microstep && (selected != previous);
    output->physical_zero_confirmation_required =
        physical_zero_confirmation_required && (selected != 0.0f);
    output->selected_nonzero = (selected != 0.0f);
    output->battery_authority_state = classify_battery_authority(
        selected, selected_steady, input, cal, false);
    if(!execution_counts_within_contract(output))
    {
        ecu_torque_clamp_force_static_zero(
            input, runtime, state, ECU_CLAMP_REASON_EXECUTION_BOUND, output);
        return true;
    }
    if(output->reason == ECU_CLAMP_REASON_NONE)
    {
        output->reason = (selected == request) ? ECU_CLAMP_REASON_NONE :
                                                 ECU_CLAMP_REASON_CURRENT_LIMIT;
    }
    output->output_valid = true;
    set_snapshot_generations(input, output);
    return true;
}

static void reset_settled_anchor(ecu_torque_clamp_state_t *state,
                                 float committed_torque_nm)
{
    state->transition.active = false;
    state->transition.settled_anchor_torque_nm = committed_torque_nm;
    state->transition.minimum_raw_commanded_torque_nm = committed_torque_nm;
    state->transition.maximum_raw_commanded_torque_nm = committed_torque_nm;
    state->transition.latest_raw_target_torque_nm = committed_torque_nm;
    state->transition.cumulative_raw_drift_nm = 0.0f;
    state->transition.maximum_settling_time_us = 0u;
    state->transition.steady_confirmation_required = 0u;
    state->transition.steady_confirmation_observed = 0u;
    state->transition.profile = ECU_TRANSITION_PROFILE_HOLD;
    state->transition.direction = ECU_TRANSITION_DIRECTION_HOLD;
    state->monitor_phase = ECU_CURRENT_MONITOR_STEADY;
    state->path_state = (committed_torque_nm == 0.0f) ?
        ECU_TORQUE_PATH_CONFIRMED_ZERO :
        ECU_TORQUE_PATH_SAME_SIGN_TRACKING;
}

void ecu_torque_clamp_note_hardware_commit(
    ecu_torque_clamp_state_t *state,
    const ecu_torque_clamp_output_t *candidate,
    float committed_torque_nm,
    ecu_torque_clamp_reason_t committed_reason,
    uint32_t commit_now_us,
    bool physical_zero_confirmed,
    bool steady_current_consistent,
    const ecu_pack_current_calibration_runtime_t *runtime)
{
    if((state == NULL) || !isfinite(committed_torque_nm))
    {
        return;
    }

    const bool runtime_valid =
        ecu_pack_current_calibration_runtime_valid(runtime);
    const ecu_pack_current_calibration_t *cal =
        runtime_valid ? runtime->calibration : NULL;

    /* Zero remains a valid hardware command even when the current-model
     * calibration becomes invalid after the APPS snapshot. Preserve physical
     * torque/sign history without dereferencing an unqualified artifact. */
    if(!runtime_valid)
    {
        const float previous = state->raw_committed_torque_nm;
        const ecu_torque_sign_t previous_sign = sign_of(previous);
        state->transition.previous_raw_committed_torque_nm = previous;
        state->raw_committed_torque_nm = committed_torque_nm;
        state->physical_zero_confirmed = physical_zero_confirmed;
        state->normalized_zero = (committed_torque_nm == 0.0f);
        state->transition.last_commit_us = commit_now_us;

        if(committed_torque_nm != 0.0f)
        {
            state->last_nonzero_committed_sign = sign_of(committed_torque_nm);
            state->path_state = ECU_TORQUE_PATH_UNKNOWN;
            state->monitor_phase = ECU_CURRENT_MONITOR_STEP_TRANSITION;
            return;
        }

        state->path_state = physical_zero_confirmed ?
            ECU_TORQUE_PATH_CONFIRMED_ZERO :
            ECU_TORQUE_PATH_WAITING_FOR_ZERO_CONFIRMATION;
        if(physical_zero_confirmed)
        {
            state->transition.active = false;
            state->transition.settled_anchor_torque_nm = 0.0f;
            state->transition.minimum_raw_commanded_torque_nm = 0.0f;
            state->transition.maximum_raw_commanded_torque_nm = 0.0f;
            state->transition.latest_raw_target_torque_nm = 0.0f;
            state->transition.cumulative_raw_drift_nm = 0.0f;
            state->monitor_phase = ECU_CURRENT_MONITOR_STEADY;
        }
        else if(previous_sign != ECU_TORQUE_SIGN_NONE)
        {
            state->transition.active = true;
            state->transition.minimum_raw_commanded_torque_nm =
                fminf(previous, 0.0f);
            state->transition.maximum_raw_commanded_torque_nm =
                fmaxf(previous, 0.0f);
            state->transition.latest_raw_target_torque_nm = 0.0f;
            state->transition.started_us = commit_now_us;
            state->transition.last_material_change_us = commit_now_us;
            state->transition.profile = ECU_TRANSITION_PROFILE_ZERO_ASSERT;
            state->transition.direction = ECU_TRANSITION_DIRECTION_TO_ZERO;
            state->monitor_phase = ECU_CURRENT_MONITOR_STEP_TRANSITION;
        }
        return;
    }
    const float previous = state->raw_committed_torque_nm;
    const float delta = committed_torque_nm - previous;
    const bool now_zero = normalized_zero(committed_torque_nm,
                                          state->normalized_zero, cal);
    const ecu_torque_sign_t previous_sign = sign_of(previous);
    const ecu_torque_sign_t prior_last_nonzero_sign =
        state->last_nonzero_committed_sign;
    const ecu_torque_sign_t new_sign = sign_of(committed_torque_nm);
    const bool candidate_matches = (candidate != NULL) &&
                                   candidate->output_valid &&
                                   (fabsf(candidate->selected_torque_nm -
                                          committed_torque_nm) < 0.0001f);
    const bool late_or_fault_zero = (committed_torque_nm == 0.0f) &&
                                    !candidate_matches;

    state->physical_zero_confirmed = physical_zero_confirmed;
    state->normalized_zero = now_zero;
    state->transition.previous_raw_committed_torque_nm = previous;
    state->raw_committed_torque_nm = committed_torque_nm;

    if(!now_zero)
    {
        const bool reversal_leg_two = candidate_matches &&
            candidate->physical_zero_confirmation_required &&
            (prior_last_nonzero_sign != ECU_TORQUE_SIGN_NONE) &&
            (new_sign != prior_last_nonzero_sign);
        const bool reducing_toward_zero = candidate_matches &&
            (candidate->transition_direction ==
             ECU_TRANSITION_DIRECTION_SAME_SIGN_DECREASE);
        state->last_nonzero_committed_sign = new_sign;
        state->path_state = reversal_leg_two ?
            ECU_TORQUE_PATH_REVERSAL_LEG_TWO :
            reducing_toward_zero ? ECU_TORQUE_PATH_RAMPING_TO_ZERO :
                                   ECU_TORQUE_PATH_SAME_SIGN_TRACKING;
    }
    else if(physical_zero_confirmed)
    {
        state->path_state = ECU_TORQUE_PATH_CONFIRMED_ZERO;
    }
    else if(previous_sign != ECU_TORQUE_SIGN_NONE)
    {
        state->path_state = ECU_TORQUE_PATH_WAITING_FOR_ZERO_CONFIRMATION;
    }
    else
    {
        state->path_state = ECU_TORQUE_PATH_WAITING_FOR_ZERO_CONFIRMATION;
    }

    bool material_change = (delta != 0.0f);
    bool microstep = false;
    ecu_transition_profile_t profile = ECU_TRANSITION_PROFILE_HOLD;
    ecu_transition_direction_t direction = ECU_TRANSITION_DIRECTION_HOLD;
    uint32_t settling_time_us = 0u;

    if(candidate_matches)
    {
        material_change = candidate->material_change;
        microstep = candidate->microstep_applied;
        profile = candidate->transition_profile;
        direction = candidate->transition_direction;
        settling_time_us = candidate->transition_maximum_settling_time_us;
    }
    else if(late_or_fault_zero)
    {
        profile = ECU_TRANSITION_PROFILE_ZERO_ASSERT;
        direction = ECU_TRANSITION_DIRECTION_TO_ZERO;
        settling_time_us = cal->late_zero_maximum_settling_time_us;
        material_change = (previous != 0.0f);
    }

    const bool extends_span = state->transition.active &&
        ((committed_torque_nm <
          state->transition.minimum_raw_commanded_torque_nm) ||
         (committed_torque_nm >
          state->transition.maximum_raw_commanded_torque_nm));
    const bool profile_changed = state->transition.active &&
        ((profile != state->transition.profile) ||
         (direction != state->transition.direction));
    if(state->transition.active && material_change &&
       !extends_span && !profile_changed && !now_zero)
    {
        /* A command decomposition already contained by the certified active
         * span does not restart the physical settling clock. */
        material_change = false;
    }

    if(microstep && !state->transition.active)
    {
        state->transition.cumulative_raw_drift_nm += fabsf(delta);
        state->monitor_phase = ECU_CURRENT_MONITOR_STEADY;
    }
    else if(material_change)
    {
        if(!state->transition.active)
        {
            state->transition.active = true;
            /* Preserve the prior settled anchor. It can differ from previous
             * after a train of certified microsteps. */
            const float anchor =
                state->transition.settled_anchor_torque_nm;
            state->transition.minimum_raw_commanded_torque_nm =
                fminf(anchor, fminf(previous, committed_torque_nm));
            state->transition.maximum_raw_commanded_torque_nm =
                fmaxf(anchor, fmaxf(previous, committed_torque_nm));
            state->transition.started_us = commit_now_us;
        }
        else
        {
            state->transition.minimum_raw_commanded_torque_nm =
                fminf(state->transition.minimum_raw_commanded_torque_nm,
                      committed_torque_nm);
            state->transition.maximum_raw_commanded_torque_nm =
                fmaxf(state->transition.maximum_raw_commanded_torque_nm,
                      committed_torque_nm);
        }
        state->transition.latest_raw_target_torque_nm = committed_torque_nm;
        state->transition.last_material_change_us = commit_now_us;
        state->transition.maximum_settling_time_us =
            (settling_time_us > state->transition.maximum_settling_time_us) ?
            settling_time_us : state->transition.maximum_settling_time_us;
        state->transition.steady_confirmation_required =
            cal->steady_confirmation_samples;
        state->transition.steady_confirmation_observed = 0u;
        state->transition.profile = profile;
        state->transition.direction = direction;
        state->monitor_phase =
            (profile == ECU_TRANSITION_PROFILE_SLEW_LIMITED ||
             profile == ECU_TRANSITION_PROFILE_COMPOSED) ?
            ECU_CURRENT_MONITOR_SLEW_TRACKING :
            ECU_CURRENT_MONITOR_STEP_TRANSITION;
    }
    else if(state->transition.active)
    {
        const float prior_target =
            state->transition.latest_raw_target_torque_nm;
        const uint32_t elapsed_since_material =
            commit_now_us - state->transition.last_material_change_us;
        const uint32_t required_time =
            (state->transition.maximum_settling_time_us >
             cal->settled_tracking_time_us) ?
            state->transition.maximum_settling_time_us :
            cal->settled_tracking_time_us;
        const uint32_t commit_delta_us =
            (state->transition.last_commit_us == 0u) ? 0u :
            (commit_now_us - state->transition.last_commit_us);
        const float command_rate = (commit_delta_us == 0u) ? INFINITY :
            (fabsf(delta) * 1000000.0f / (float)commit_delta_us);
        const bool tracking_stable =
            (fabsf(committed_torque_nm - prior_target) <=
             cal->tracking_band_nm) &&
            (command_rate <= cal->maximum_settled_command_rate_nm_per_s);
        state->transition.latest_raw_target_torque_nm = committed_torque_nm;

        if((elapsed_since_material >= required_time) && tracking_stable &&
           steady_current_consistent)
        {
            if(state->transition.steady_confirmation_observed < UINT16_MAX)
            {
                state->transition.steady_confirmation_observed++;
            }
            state->monitor_phase = ECU_CURRENT_MONITOR_SETTLING;
            if(state->transition.steady_confirmation_observed >=
               state->transition.steady_confirmation_required)
            {
                reset_settled_anchor(state, committed_torque_nm);
            }
        }
        else
        {
            state->transition.steady_confirmation_observed = 0u;
            state->monitor_phase = ECU_CURRENT_MONITOR_SETTLING;
        }
    }
    else if((state->path_state == ECU_TORQUE_PATH_CONFIRMED_ZERO) ||
            (state->path_state == ECU_TORQUE_PATH_SAME_SIGN_TRACKING))
    {
        if(state->transition.settled_anchor_torque_nm == 0.0f &&
           state->transition.last_commit_us == 0u)
        {
            reset_settled_anchor(state, committed_torque_nm);
        }
    }

    state->transition.cumulative_raw_drift_nm +=
        (!microstep && state->transition.active) ? fabsf(delta) : 0.0f;
    state->transition.last_commit_us = commit_now_us;
    (void)committed_reason;
}

bool ecu_torque_clamp_commit_reverify(
    const ecu_torque_clamp_output_t *candidate,
    const ecu_torque_commit_verification_t *verification,
    float *committed_torque_nm,
    ecu_torque_clamp_reason_t *reason)
{
    if((candidate == NULL) || (verification == NULL) ||
       (committed_torque_nm == NULL) || (reason == NULL) ||
       !candidate->output_valid)
    {
        if(committed_torque_nm != NULL)
        {
            *committed_torque_nm = 0.0f;
        }
        if(reason != NULL)
        {
            *reason = ECU_CLAMP_REASON_INPUT_INVALID;
        }
        return false;
    }

    /* Zero is always commandable and never depends on current-direction,
     * calibration, or authority validity. */
    if(candidate->selected_torque_nm == 0.0f)
    {
        *committed_torque_nm = 0.0f;
        *reason = candidate->reason;
        return true;
    }

    if(!verification->calibration_valid)
    {
        *committed_torque_nm = 0.0f;
        *reason = ECU_CLAMP_REASON_CALIBRATION_INVALID;
        return false;
    }
    if(!verification->authority_valid || !verification->safety_gate_valid)
    {
        *committed_torque_nm = 0.0f;
        *reason = verification->authority_valid ?
            ECU_CLAMP_REASON_INPUT_INVALID :
            ECU_CLAMP_REASON_AUTHORITY_INVALID;
        return false;
    }

    if(candidate->physical_zero_confirmation_required &&
       !verification->physical_zero_confirmed)
    {
        *committed_torque_nm = 0.0f;
        *reason = ECU_CLAMP_REASON_REVERSAL_WAIT;
        return false;
    }

    if((candidate->speed_generation != verification->speed_generation) ||
       (candidate->vdc_generation != verification->vdc_generation) ||
       (candidate->temperature_generation !=
        verification->temperature_generation) ||
       (candidate->calibration_generation !=
        verification->calibration_generation))
    {
        *committed_torque_nm = 0.0f;
        *reason = ECU_CLAMP_REASON_OPERATING_POINT_CHANGED;
        return false;
    }

    if((uint32_t)(verification->commit_timestamp_us -
                  candidate->computation_timestamp_us) >
       ECU_CLAMP_MAX_CONTRACT_AGE_US)
    {
        *committed_torque_nm = 0.0f;
        *reason = ECU_CLAMP_REASON_DEADLINE_OVERRUN;
        return false;
    }

    if((candidate->capability_generation !=
        verification->capability_generation) ||
       (candidate->selected_torque_nm > verification->cm200_positive_cap_nm) ||
       (candidate->selected_torque_nm < verification->cm200_negative_cap_nm))
    {
        *committed_torque_nm = 0.0f;
        *reason = ECU_CLAMP_REASON_CM200_CAPABILITY_CHANGED;
        return false;
    }

    if(!interval_authorized(candidate->steady_current_a,
                            verification->dcl_a,
                            verification->ccl_a,
                            verification->discharge_authorized,
                            verification->charge_authorized) ||
       (candidate->transition_authority_required &&
        (!candidate->transition_interval_valid ||
         !interval_authorized(candidate->transition_current_a,
                              verification->dcl_a,
                              verification->ccl_a,
                              verification->discharge_authorized,
                              verification->charge_authorized))))
    {
        *committed_torque_nm = 0.0f;
        *reason = ECU_CLAMP_REASON_LATE_AUTHORITY_TIGHTENING;
        return false;
    }

    *committed_torque_nm = candidate->selected_torque_nm;
    *reason = candidate->reason;
    return true;
}
