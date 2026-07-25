#include "power/ecu_current_residual_monitor.h"

#include <math.h>
#include <string.h>

static uint32_t modular_timestamp_distance_us(uint32_t first,
                                              uint32_t second)
{
    const uint32_t forward = first - second;
    const uint32_t backward = second - first;
    return (forward < backward) ? forward : backward;
}

void ecu_current_residual_monitor_init(ecu_current_residual_monitor_t *monitor)
{
    if(monitor != NULL)
    {
        memset(monitor, 0, sizeof(*monitor));
        monitor->phase = ECU_CURRENT_MONITOR_STEP_TRANSITION;
    }
}

bool ecu_current_residual_monitor_update(
    ecu_current_residual_monitor_t *monitor,
    const ecu_current_residual_input_t *input,
    const ecu_current_residual_config_t *config)
{
    if((monitor == NULL) || (input == NULL) || (config == NULL) ||
       (config->trip_samples == 0u) || (config->clear_samples == 0u) ||
       !isfinite(config->measurement_uncertainty_negative_a) ||
       !isfinite(config->measurement_uncertainty_positive_a) ||
       (config->measurement_uncertainty_negative_a < 0.0f) ||
       (config->measurement_uncertainty_positive_a < 0.0f))
    {
        return true;
    }

    if(!monitor->source_epoch_seen ||
       (monitor->source_epoch != input->source_epoch))
    {
        monitor->source_epoch = input->source_epoch;
        monitor->source_epoch_seen = true;
        monitor->measurement_sequence_seen = false;
        monitor->last_measurement_sequence = 0u;
        monitor->violation_count = 0u;
        monitor->recovery_count = 0u;
        monitor->source_settling_count = config->source_settling_samples;
        monitor->phase = ECU_CURRENT_MONITOR_SETTLING;
        monitor->latest_sample_valid = false;
        monitor->latest_sample_within_envelope = false;
        return monitor->fault_latched;
    }

    const uint32_t measurement_age_us =
        input->now_us - input->measurement_timestamp_us;

    /* Measurement availability is a time-domain safety condition, not a
     * sample-persistence condition. A stream that stops must not wait for
     * repeated scheduler executions to manufacture N "bad samples". */
    if(!input->measurement_valid ||
       (measurement_age_us > config->maximum_measurement_age_us))
    {
        monitor->latest_sample_valid = false;
        monitor->latest_sample_within_envelope = false;
        monitor->recovery_count = 0u;
        monitor->fault_latched = true;
        return true;
    }

    const bool new_measurement =
        !monitor->measurement_sequence_seen ||
        (monitor->last_measurement_sequence != input->measurement_sequence);
    if(!new_measurement)
    {
        /* The CAN task may execute many times per AMS publication. Persistence
         * counters represent distinct physical samples, never loop count. */
        return monitor->fault_latched;
    }

    monitor->measurement_sequence_seen = true;
    monitor->last_measurement_sequence = input->measurement_sequence;
    monitor->phase = input->phase;

    if(monitor->source_settling_count > 0u)
    {
        monitor->source_settling_count--;
        monitor->violation_count = 0u;
        monitor->recovery_count = 0u;
        monitor->latest_sample_valid = false;
        monitor->latest_sample_within_envelope = false;
        return monitor->fault_latched;
    }

    const uint32_t alignment_error_us = modular_timestamp_distance_us(
        input->measurement_timestamp_us, input->command_timestamp_us);
    const bool valid = input->prediction_valid &&
        isfinite(input->measured_pack_current_a) &&
        isfinite(input->predicted_pack_current_a.min_a) &&
        isfinite(input->predicted_pack_current_a.max_a) &&
        (input->predicted_pack_current_a.min_a <=
         input->predicted_pack_current_a.max_a) &&
        (alignment_error_us <= config->maximum_alignment_error_us);

    const bool violation = !valid ||
        (input->measured_pack_current_a <
         (input->predicted_pack_current_a.min_a -
          config->measurement_uncertainty_negative_a)) ||
        (input->measured_pack_current_a >
         (input->predicted_pack_current_a.max_a +
          config->measurement_uncertainty_positive_a));

    monitor->latest_sample_valid = valid;
    monitor->latest_sample_within_envelope = valid && !violation;

    if(violation)
    {
        monitor->recovery_count = 0u;
        if(monitor->violation_count < UINT16_MAX)
        {
            monitor->violation_count++;
        }
        if(monitor->violation_count >= config->trip_samples)
        {
            monitor->fault_latched = true;
        }
    }
    else
    {
        monitor->violation_count = 0u;
        if(monitor->recovery_count < UINT16_MAX)
        {
            monitor->recovery_count++;
        }
        if(!config->latch_until_reset &&
           (monitor->recovery_count >= config->clear_samples))
        {
            monitor->fault_latched = false;
        }
    }

    return monitor->fault_latched;
}
