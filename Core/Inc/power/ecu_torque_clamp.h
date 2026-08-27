#ifndef ECU_TORQUE_CLAMP_H_
#define ECU_TORQUE_CLAMP_H_

#include <stdbool.h>
#include <stdint.h>

#include "power/ecu_pack_current_model.h"

#define ECU_CLAMP_MAX_TRANSITION_REFINE_ITERS 4u

/* These bounds are tied to the maximum schema accepted by the boot loader.
 * They are deliberately conservative and are exercised by host property tests.
 * The final hardware-mailbox commit performs zero model calls. */
#define ECU_CLAMP_MAX_STEADY_MODEL_CALLS       32u
#define ECU_CLAMP_MAX_TRANSITION_MODEL_CALLS    8u
#define ECU_CLAMP_MAX_TORQUE_CELL_EVALUATIONS  64u
#define ECU_CLAMP_MAX_COMMIT_MODEL_CALLS         0u
#define ECU_CLAMP_CONTROL_PERIOD_US          10000u
#define ECU_CLAMP_MAX_CONTRACT_AGE_US        10000u

_Static_assert(ECU_CURRENT_MODEL_MAX_TORQUE_CELLS <= 20u,
               "Clamp WCET contract assumes at most 20 torque cells");

typedef enum
{
    ECU_TORQUE_SIGN_NONE = 0,
    ECU_TORQUE_SIGN_POSITIVE,
    ECU_TORQUE_SIGN_NEGATIVE
} ecu_torque_sign_t;

typedef enum
{
    ECU_TORQUE_PATH_UNKNOWN = 0,
    ECU_TORQUE_PATH_CONFIRMED_ZERO,
    ECU_TORQUE_PATH_SAME_SIGN_TRACKING,
    ECU_TORQUE_PATH_RAMPING_TO_ZERO,
    ECU_TORQUE_PATH_WAITING_FOR_ZERO_CONFIRMATION,
    ECU_TORQUE_PATH_REVERSAL_LEG_TWO
} ecu_torque_path_state_t;

typedef enum
{
    ECU_BATTERY_AUTHORITY_NORMAL = 0,
    ECU_BATTERY_AUTHORITY_LOW,
    ECU_BATTERY_AUTHORITY_TORQUE_EXHAUSTED,
    ECU_BATTERY_AUTHORITY_ZERO_STEADY_AUX_INFEASIBLE
} ecu_battery_authority_state_t;

typedef enum
{
    ECU_CLAMP_REASON_NONE = 0,
    ECU_CLAMP_REASON_ZERO_REQUEST,
    ECU_CLAMP_REASON_INPUT_INVALID,
    ECU_CLAMP_REASON_CALIBRATION_INVALID,
    ECU_CLAMP_REASON_AUTHORITY_INVALID,
    ECU_CLAMP_REASON_DIRECTION_INHIBIT,
    ECU_CLAMP_REASON_CURRENT_LIMIT,
    ECU_CLAMP_REASON_REVERSAL_WAIT,
    ECU_CLAMP_REASON_SEARCH_EXHAUSTED,
    ECU_CLAMP_REASON_LATE_AUTHORITY_TIGHTENING,
    ECU_CLAMP_REASON_TRANSITION_LIMIT,
    ECU_CLAMP_REASON_OPERATING_POINT_CHANGED,
    ECU_CLAMP_REASON_CM200_CAPABILITY_CHANGED,
    ECU_CLAMP_REASON_CURRENT_MODEL_RESIDUAL,
    ECU_CLAMP_REASON_EXECUTION_BOUND,
    ECU_CLAMP_REASON_DEADLINE_OVERRUN
} ecu_torque_clamp_reason_t;

typedef enum
{
    ECU_CURRENT_MONITOR_STEP_TRANSITION = 0,
    ECU_CURRENT_MONITOR_SLEW_TRACKING,
    ECU_CURRENT_MONITOR_SETTLING,
    ECU_CURRENT_MONITOR_STEADY
} ecu_current_monitor_phase_t;

typedef struct
{
    bool active;
    float settled_anchor_torque_nm;
    float minimum_raw_commanded_torque_nm;
    float maximum_raw_commanded_torque_nm;
    float latest_raw_target_torque_nm;
    float cumulative_raw_drift_nm;
    float previous_raw_committed_torque_nm;
    uint32_t started_us;
    uint32_t last_material_change_us;
    uint32_t last_commit_us;
    uint32_t maximum_settling_time_us;
    uint16_t steady_confirmation_required;
    uint16_t steady_confirmation_observed;
    ecu_transition_profile_t profile;
    ecu_transition_direction_t direction;
} ecu_active_transition_t;

typedef struct
{
    ecu_torque_path_state_t path_state;
    ecu_torque_sign_t last_nonzero_committed_sign;
    float raw_committed_torque_nm;
    bool normalized_zero;
    bool physical_zero_confirmed;
    ecu_current_monitor_phase_t monitor_phase;
    ecu_active_transition_t transition;
} ecu_torque_clamp_state_t;

typedef struct
{
    float requested_torque_nm;
    float motor_speed_rpm;
    float dc_bus_voltage_v;
    float inverter_temp_c;
    float motor_temp_c;
    float cm200_positive_cap_nm;
    float cm200_negative_cap_nm;
    float dcl_a;
    float ccl_a;
    uint32_t now_us;
    uint32_t motor_speed_age_us;
    uint32_t dc_bus_voltage_age_us;
    uint32_t inverter_temp_age_us;
    uint32_t motor_temp_age_us;
    uint32_t speed_generation;
    uint32_t vdc_generation;
    uint32_t temperature_generation;
    uint32_t capability_generation;
    uint32_t authority_received_ms;
    uint32_t calibration_generation;
    uint8_t authority_counter;
    bool discharge_authorized;
    bool charge_authorized;
    bool authority_valid;
    bool operating_point_valid;
    bool physical_zero_confirmed;
} ecu_torque_clamp_input_t;

typedef struct
{
    float selected_torque_nm;
    ecu_current_interval_t steady_current_a;
    ecu_current_interval_t transition_current_a;
    ecu_current_interval_t zero_fallback_transition_current_a;
    ecu_torque_clamp_reason_t reason;
    ecu_battery_authority_state_t battery_authority_state;
    ecu_transition_profile_t transition_profile;
    ecu_transition_direction_t transition_direction;
    uint32_t transition_maximum_settling_time_us;
    uint32_t zero_fallback_maximum_settling_time_us;
    uint32_t computation_timestamp_us;
    uint32_t speed_generation;
    uint32_t vdc_generation;
    uint32_t temperature_generation;
    uint32_t capability_generation;
    uint32_t calibration_generation;
    uint16_t steady_model_calls;
    uint16_t transition_model_calls;
    uint16_t torque_cells_evaluated;
    uint16_t boundary_refinement_iterations;
    uint16_t transition_refinement_iterations;
    bool selected_nonzero;
    bool transition_interval_valid;
    bool transition_authority_required;
    bool microstep_applied;
    bool material_change;
    bool physical_zero_confirmation_required;
    bool zero_fallback_valid;
    bool output_valid;
} ecu_torque_clamp_output_t;

typedef struct
{
    float dcl_a;
    float ccl_a;
    float cm200_positive_cap_nm;
    float cm200_negative_cap_nm;
    uint32_t speed_generation;
    uint32_t vdc_generation;
    uint32_t temperature_generation;
    uint32_t capability_generation;
    uint32_t calibration_generation;
    uint32_t commit_timestamp_us;
    bool discharge_authorized;
    bool charge_authorized;
    bool authority_valid;
    bool calibration_valid;
    bool safety_gate_valid;
    bool physical_zero_confirmed;
} ecu_torque_commit_verification_t;

typedef struct
{
    ecu_torque_clamp_output_t candidate;
    ecu_torque_clamp_input_t input_snapshot;
    bool valid;
} ecu_torque_command_contract_t;

void ecu_torque_clamp_state_init(ecu_torque_clamp_state_t *state);

bool ecu_torque_clamp_run(
    const ecu_torque_clamp_input_t *input,
    const ecu_pack_current_calibration_runtime_t *runtime,
    const ecu_torque_clamp_state_t *state,
    ecu_torque_clamp_output_t *output);

void ecu_torque_clamp_note_hardware_commit(
    ecu_torque_clamp_state_t *state,
    const ecu_torque_clamp_output_t *candidate,
    float committed_torque_nm,
    ecu_torque_clamp_reason_t committed_reason,
    uint32_t commit_now_us,
    bool physical_zero_confirmed,
    bool steady_current_consistent,
    const ecu_pack_current_calibration_runtime_t *runtime);

void ecu_torque_clamp_force_static_zero(
    const ecu_torque_clamp_input_t *input,
    const ecu_pack_current_calibration_runtime_t *runtime,
    const ecu_torque_clamp_state_t *state,
    ecu_torque_clamp_reason_t reason,
    ecu_torque_clamp_output_t *output);

bool ecu_torque_clamp_commit_reverify(
    const ecu_torque_clamp_output_t *candidate,
    const ecu_torque_commit_verification_t *verification,
    float *committed_torque_nm,
    ecu_torque_clamp_reason_t *reason);

#endif
