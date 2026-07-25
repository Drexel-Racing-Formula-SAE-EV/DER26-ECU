#include <assert.h>
#include <stdio.h>

#include "power/ecu_current_residual_monitor.h"

static bool push_new(ecu_current_residual_monitor_t *monitor,
                     ecu_current_residual_input_t *input,
                     const ecu_current_residual_config_t *config)
{
    input->measurement_sequence++;
    return ecu_current_residual_monitor_update(monitor, input, config);
}

int main(void)
{
    ecu_current_residual_monitor_t monitor;
    ecu_current_residual_monitor_init(&monitor);
    const ecu_current_residual_config_t config = {
        .trip_samples = 3u,
        .clear_samples = 2u,
        .source_settling_samples = 2u,
        .maximum_measurement_age_us = 50000u,
        .maximum_alignment_error_us = 20000u,
        .measurement_uncertainty_negative_a = 0.5f,
        .measurement_uncertainty_positive_a = 0.5f,
        .latch_until_reset = true,
    };
    ecu_current_residual_input_t input = {
        .measured_pack_current_a = 5.0f,
        .predicted_pack_current_a = {0.0f, 10.0f},
        .phase = ECU_CURRENT_MONITOR_STEADY,
        .measurement_timestamp_us = 100000u,
        .command_timestamp_us = 95000u,
        .now_us = 101000u,
        .source_epoch = 1u,
        .measurement_sequence = 0u,
        .measurement_valid = true,
        .prediction_valid = true,
    };

    /* New source epoch starts a bounded settling period. */
    assert(!ecu_current_residual_monitor_update(&monitor, &input, &config));
    assert(monitor.phase == ECU_CURRENT_MONITOR_SETTLING);
    assert(!push_new(&monitor, &input, &config));
    assert(!push_new(&monitor, &input, &config));
    assert(!push_new(&monitor, &input, &config));
    assert(monitor.latest_sample_valid);
    assert(monitor.latest_sample_within_envelope);

    /* Re-evaluating the same physical sample never advances persistence. */
    input.measured_pack_current_a = 20.0f;
    assert(!ecu_current_residual_monitor_update(&monitor, &input, &config));
    assert(monitor.violation_count == 0u);

    input.measured_pack_current_a = 10.4f;
    assert(!push_new(&monitor, &input, &config));
    assert(monitor.latest_sample_within_envelope);

    input.measured_pack_current_a = 20.0f;
    assert(!push_new(&monitor, &input, &config));
    assert(!ecu_current_residual_monitor_update(&monitor, &input, &config));
    assert(monitor.violation_count == 1u);
    assert(!push_new(&monitor, &input, &config));
    assert(push_new(&monitor, &input, &config));
    assert(monitor.latest_sample_valid);
    assert(!monitor.latest_sample_within_envelope);

    /* Latching means good data cannot clear the fault. */
    input.measured_pack_current_a = 5.0f;
    assert(push_new(&monitor, &input, &config));

    /* A source change clears persistence, not the latched fault. */
    input.source_epoch = 2u;
    assert(ecu_current_residual_monitor_update(&monitor, &input, &config));
    assert(monitor.violation_count == 0u);

    /* Stale data is a time-domain fault and does not wait for fake samples. */
    ecu_current_residual_monitor_init(&monitor);
    input.source_epoch = 3u;
    input.measurement_sequence = 0u;
    input.now_us = 200000u;
    input.measurement_timestamp_us = 100000u;
    assert(!ecu_current_residual_monitor_update(&monitor, &input, &config));
    assert(ecu_current_residual_monitor_update(&monitor, &input, &config));

    /* Command/measurement alignment remains valid across the 32-bit
     * microsecond timestamp wrap. */
    ecu_current_residual_monitor_init(&monitor);
    input.source_epoch = 4u;
    input.measurement_sequence = 0u;
    input.measurement_valid = true;
    input.prediction_valid = true;
    input.measured_pack_current_a = 5.0f;
    input.predicted_pack_current_a = (ecu_current_interval_t){0.0f, 10.0f};
    input.command_timestamp_us = UINT32_MAX - 5000u;
    input.measurement_timestamp_us = 4000u;
    input.now_us = 5000u;
    assert(!ecu_current_residual_monitor_update(&monitor, &input, &config));
    assert(!push_new(&monitor, &input, &config));
    assert(!push_new(&monitor, &input, &config));
    assert(!push_new(&monitor, &input, &config));
    assert(monitor.latest_sample_valid);

    puts("PASS current residual monitor tests");
    return 0;
}
