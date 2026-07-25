#ifndef ECU_CURRENT_RESIDUAL_MONITOR_H_
#define ECU_CURRENT_RESIDUAL_MONITOR_H_

#include <stdbool.h>
#include <stdint.h>

#include "power/ecu_torque_clamp.h"

typedef struct
{
    ecu_current_monitor_phase_t phase;
    uint32_t source_epoch;
    uint16_t violation_count;
    uint16_t recovery_count;
    uint16_t source_settling_count;
    uint32_t last_measurement_sequence;
    bool source_epoch_seen;
    bool measurement_sequence_seen;
    bool latest_sample_valid;
    bool latest_sample_within_envelope;
    bool fault_latched;
} ecu_current_residual_monitor_t;

typedef struct
{
    ecu_current_interval_t predicted_pack_current_a;
    ecu_current_monitor_phase_t phase;
    uint32_t command_timestamp_us;
    bool valid;
} ecu_current_prediction_snapshot_t;

typedef struct
{
    float measured_pack_current_a;
    ecu_current_interval_t predicted_pack_current_a;
    ecu_current_monitor_phase_t phase;
    uint32_t measurement_timestamp_us;
    uint32_t command_timestamp_us;
    uint32_t now_us;
    uint32_t source_epoch;
    uint32_t measurement_sequence;
    bool measurement_valid;
    bool prediction_valid;
} ecu_current_residual_input_t;

typedef struct
{
    uint16_t trip_samples;
    uint16_t clear_samples;
    uint16_t source_settling_samples;
    uint32_t maximum_measurement_age_us;
    uint32_t maximum_alignment_error_us;
    float measurement_uncertainty_negative_a;
    float measurement_uncertainty_positive_a;
    bool latch_until_reset;
} ecu_current_residual_config_t;

void ecu_current_residual_monitor_init(ecu_current_residual_monitor_t *monitor);
bool ecu_current_residual_monitor_update(
    ecu_current_residual_monitor_t *monitor,
    const ecu_current_residual_input_t *input,
    const ecu_current_residual_config_t *config);

#endif
