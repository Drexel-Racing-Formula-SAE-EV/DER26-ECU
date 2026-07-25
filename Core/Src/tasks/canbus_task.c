/**
 * @file canbus_task.c
 * @author Cole Bardin (cab572@drexel.edu)
 * @brief Final CM200 mailbox commit and CAN receive/transmit service.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "tasks/canbus_task.h"
#include "ext_drivers/canbus.h"
#include "ext_drivers/cm200.h"
#include "ext_drivers/ecu_safety.h"
#include "power/ecu_pack_current_calibration.h"

#define CANBUS_COMMAND_WAIT_MS 25u

static const ecu_current_residual_config_t current_residual_config = {
    .trip_samples = 3u,
    .clear_samples = 20u,
    .source_settling_samples = 5u,
    .maximum_measurement_age_us = 200000u,
    .maximum_alignment_error_us = 150000u,
    /* Provisional. Replace with the worst certified DHAB/APM canonical
     * measurement interval before vehicle validation. */
    .measurement_uncertainty_negative_a = 0.0f,
    .measurement_uncertainty_positive_a = 0.0f,
    .latch_until_reset = true,
};

void canbus_task_fn(void *arg);

static StaticTask_t canbus_task_tcb;
static StackType_t canbus_task_stack[ECU_STACK_CAN_WORDS];
static TaskHandle_t canbus_task_handle = NULL;

static int16_t torque_nm_to_0p1nm(float torque_nm)
{
    const float scaled = torque_nm * 10.0f;
    if(scaled >= 32767.0f)
    {
        return INT16_MAX;
    }
    if(scaled <= -32768.0f)
    {
        return INT16_MIN;
    }
    /* Clamp output is already quantized toward zero. Round only to recover
     * the intended integer from binary floating-point representation. */
    return (int16_t)lrintf(scaled);
}

static ecu_current_interval_t interval_union(ecu_current_interval_t first,
                                              ecu_current_interval_t second)
{
    ecu_current_interval_t result = first;
    if(second.min_a < result.min_a)
    {
        result.min_a = second.min_a;
    }
    if(second.max_a > result.max_a)
    {
        result.max_a = second.max_a;
    }
    return result;
}

static bool safety_gate_valid_locked(const app_data_t *data)
{
    const ecu_torque_inputs_t inputs = {
        .cascadia_ok = data->cascadia_ok,
        .hard_fault = data->hard_fault || data->current_model_residual_fault,
        .apps_fault = data->apps_fault,
        .bppc_fault = data->bppc_fault,
        .bse_fault = data->bse_fault,
        .ams_fault = data->ams_fault,
        .canbus_fault = data->canbus_fault || data->canbus_hw_fault,
        .canbus_rx_fault = data->canbus_rx_fault,
        .canbus_tx_fault = data->canbus_tx_fault,
        .imd_fail = data->imd_fail,
        .bms_fail = data->bms_fail,
        .bspd_fail = data->bspd_fail,
        .cm200_fault = data->cm200_fault || !data->cm200_ready,
        .rtd_mode = data->rtd_mode,
    };

    return !ECU_OUTPUTS_INHIBITED &&
           ams_allows_torque(&data->board.ams) &&
           cm200_allows_torque(&data->board.cm200) &&
           ecu_torque_allowed(&inputs);
}

static bool physical_zero_confirmed_locked(const app_data_t *data)
{
    const cm200_frame_health_t *internal =
        &data->board.cm200.frame[CM200_FRAME_INTERNAL_STATES];
    const cm200_frame_health_t *torque =
        &data->board.cm200.frame[CM200_FRAME_TORQUE_TIMER];
    const bool disabled_confirmed = internal->valid && internal->sane &&
                                    !internal->stale &&
                                    !data->board.cm200.inverter_enabled;
    const float threshold_nm = g_ecu_pack_current_calibration.zero_enter_nm;
    const bool feedback_confirmed = torque->valid && torque->sane &&
                                    !torque->stale &&
        (abs(data->board.cm200.torque_feedback_0p1nm) <=
         (int16_t)(threshold_nm * 10.0f));
    return disabled_confirmed || feedback_confirmed;
}

static void publish_prediction_locked(app_data_t *data,
                                      const ecu_torque_clamp_output_t *candidate,
                                      float committed_torque_nm,
                                      uint32_t commit_now_us)
{
    ecu_current_prediction_snapshot_t prediction;
    memset(&prediction, 0, sizeof(prediction));
    prediction.command_timestamp_us = commit_now_us;
    prediction.phase = data->torque_clamp_state.monitor_phase;

    if((candidate != NULL) && candidate->output_valid)
    {
        if((fabsf(committed_torque_nm - candidate->selected_torque_nm) <
            0.0001f) && candidate->selected_nonzero)
        {
            prediction.predicted_pack_current_a = candidate->steady_current_a;
            if(candidate->transition_interval_valid)
            {
                prediction.predicted_pack_current_a = interval_union(
                    prediction.predicted_pack_current_a,
                    candidate->transition_current_a);
            }
            prediction.valid = true;
        }
        else if(committed_torque_nm == 0.0f)
        {
            if((candidate->selected_torque_nm == 0.0f) &&
               candidate->steady_current_a.min_a <=
                   candidate->steady_current_a.max_a)
            {
                prediction.predicted_pack_current_a = candidate->steady_current_a;
                if(candidate->zero_fallback_valid)
                {
                    prediction.predicted_pack_current_a = interval_union(
                        prediction.predicted_pack_current_a,
                        candidate->zero_fallback_transition_current_a);
                }
                prediction.valid = true;
            }
            else if(candidate->zero_fallback_valid)
            {
                prediction.predicted_pack_current_a =
                    candidate->zero_fallback_transition_current_a;
                prediction.valid = true;
            }
        }
    }

    if(!prediction.valid && (committed_torque_nm == 0.0f) &&
       ecu_pack_current_calibration_runtime_valid(
           &data->pack_current_calibration_runtime))
    {
        prediction.predicted_pack_current_a =
            data->pack_current_calibration_runtime.calibration
                ->late_zero_transition_current_a;
        prediction.valid = true;
    }

    data->current_prediction = prediction;
}

static void update_current_residual_monitor(app_data_t *data, uint32_t now_ms)
{
    ecu_current_prediction_snapshot_t prediction;
    int16_t measured_0p1a;
    uint32_t measurement_tick_ms;
    uint32_t source_epoch;
    uint32_t measurement_sequence;
    bool measurement_valid;

    bool wait_for_current_diag = false;

    taskENTER_CRITICAL();
    prediction = data->current_prediction;
    measured_0p1a = data->board.ams.pack_current_0p1a;
    measurement_tick_ms = data->board.ams.last_electrical_rx_tick;
    measurement_valid = data->board.ams.current_valid &&
                        data->board.ams.compact_electrical_valid &&
                        !data->board.ams.compact_electrical_stale;
    source_epoch = data->current_source_epoch;
    measurement_sequence = data->board.ams.compact_electrical_sequence;

    if(data->board.ams.current_diag_rx_count != 0u)
    {
        const bool diagnostic_is_current =
            data->board.ams.current_diag_valid &&
            data->board.ams.current_diag_sane &&
            ((int32_t)(data->board.ams.last_current_diag_rx_tick -
                       data->board.ams.last_electrical_rx_tick) >= 0);
        if(diagnostic_is_current)
        {
            data->current_source_epoch =
                (uint32_t)data->board.ams.current_source_epoch;
            source_epoch = data->current_source_epoch;
            measurement_sequence =
                (uint32_t)data->board.ams.current_sample_sequence_low;
            measurement_tick_ms =
                data->board.ams.current_physical_sample_tick;
        }
        else if((uint32_t)(now_ms -
                           data->board.ams.last_current_diag_rx_tick) <=
                (current_residual_config.maximum_measurement_age_us / 1000u))
        {
            /* The 0x68B frame follows 0x681 in the same AMS bundle. Defer one
             * monitor update rather than pairing new current with old source
             * metadata or double-counting the same physical sample. */
            wait_for_current_diag = true;
        }
        else
        {
            measurement_valid = false;
        }
    }
    taskEXIT_CRITICAL();

    if(wait_for_current_diag)
    {
        return;
    }

    const ecu_current_residual_input_t residual_input = {
        .measured_pack_current_a = (float)measured_0p1a * 0.1f,
        .predicted_pack_current_a = prediction.predicted_pack_current_a,
        .phase = prediction.phase,
        .measurement_timestamp_us = measurement_tick_ms * 1000u,
        .command_timestamp_us = prediction.command_timestamp_us,
        .now_us = now_ms * 1000u,
        .source_epoch = source_epoch,
        .measurement_sequence = measurement_sequence,
        .measurement_valid = measurement_valid,
        .prediction_valid = prediction.valid,
    };

    const bool fault = ecu_current_residual_monitor_update(
        &data->current_residual_monitor,
        &residual_input,
        &current_residual_config);
    data->current_model_residual_fault = fault;
    data->current_residual_violation_count =
        data->current_residual_monitor.violation_count;
}

TaskHandle_t canbus_task_start(app_data_t *data)
{
    if(data == NULL)
    {
        return NULL;
    }

    if(canbus_task_handle == NULL)
    {
        canbus_task_handle = xTaskCreateStatic(canbus_task_fn,
            "CANBus Task", ECU_STACK_CAN_WORDS, (void *)data, CAN_PRIO,
            canbus_task_stack, &canbus_task_tcb);
    }
    return canbus_task_handle;
}

void canbus_task_fn(void *arg)
{
    app_data_t *data = (app_data_t *)arg;
    if(data == NULL)
    {
        vTaskDelete(NULL);
        return;
    }

    canbus_t *canbus = &data->board.canbus;

    for(;;)
    {
        data->can_heartbeat_tick = osKernelGetTickCount();
        if(canbus->tx_queue == NULL)
        {
            data->canbus_tx_fault = true;
            osDelay(10u);
            continue;
        }

        canbus_tx_request_t request;
        memset(&request, 0, sizeof(request));
        request.packet.id = CM_CANBUS_ID;
        ecu_cm200_build_disable_packet(request.packet.data);

        if(xQueueReceive(canbus->tx_queue, &request,
                         pdMS_TO_TICKS(CANBUS_COMMAND_WAIT_MS)) != pdPASS)
        {
            /* APPS task loss must actively replace the last torque command. */
            request.torque_contract_valid = false;
            ecu_cm200_build_disable_packet(request.packet.data);
        }

        HAL_StatusTypeDef tx_status =
            canbus_wait_tx_ready(canbus, CANBUS_TX_TIMEOUT_MS);
        if(tx_status != HAL_OK)
        {
            data->canbus_tx_fault = true;
            update_current_residual_monitor(data, HAL_GetTick());
            continue;
        }

        if(request.packet.id == CM_CANBUS_ID)
        {
            const uint8_t transmitted_counter = data->cm200_rolling_counter;
            float committed_torque_nm = 0.0f;
            ecu_torque_clamp_reason_t commit_reason =
                ECU_CLAMP_REASON_INPUT_INVALID;
            const ecu_torque_clamp_output_t *candidate = NULL;
            uint32_t commit_now_ms;

            /* Final protected commit: after mailbox wait, freeze CAN RX and
             * task-owned safety state, obtain a coherent latest snapshot,
             * compare cached intervals only, then enqueue the actual packet. */
            taskENTER_CRITICAL();
            commit_now_ms = HAL_GetTick();

            der26_power_immediate_authority_t authority;
            memset(&authority, 0, sizeof(authority));
            const bool authority_valid = ams_get_immediate_power_authority(
                &data->board.ams, commit_now_ms, &authority);

            const ecu_torque_commit_verification_t verification = {
                .dcl_a = authority.discharge.current_limit_a,
                .ccl_a = authority.charge_regen.current_limit_a,
                .cm200_positive_cap_nm =
                    (float)data->board.cm200.motor_torque_available_0p1nm * 0.1f,
                .cm200_negative_cap_nm =
                    (float)data->board.cm200.regen_torque_available_0p1nm * 0.1f,
                .speed_generation = data->board.cm200
                    .frame[CM200_FRAME_MOTOR_POSITION].rx_count,
                .vdc_generation = data->board.cm200
                    .frame[CM200_FRAME_VOLTAGE].rx_count,
                .temperature_generation = data->board.cm200
                    .frame[CM200_FRAME_TEMPERATURES_3].rx_count,
                .capability_generation = data->board.cm200
                    .frame[CM200_FRAME_TORQUE_CAPABILITY].rx_count,
                .calibration_generation =
                    data->pack_current_calibration_runtime.generation,
                .commit_timestamp_us = commit_now_ms * 1000u,
                .discharge_authorized =
                    authority.discharge.authorized != 0u,
                .charge_authorized =
                    authority.charge_regen.authorized != 0u,
                .authority_valid = authority_valid,
                .calibration_valid =
                    ecu_pack_current_calibration_runtime_valid(
                        &data->pack_current_calibration_runtime),
                .safety_gate_valid = safety_gate_valid_locked(data),
                .physical_zero_confirmed =
                    physical_zero_confirmed_locked(data),
            };

            if(data->current_model_residual_fault)
            {
                /* No independently calibrated degraded torque cap exists in
                 * this release.  A latched total-envelope residual therefore
                 * forces the protected commit to zero and is reported with a
                 * dedicated reason.  The final commit still performs no model
                 * call and no torque search. */
                commit_reason = ECU_CLAMP_REASON_CURRENT_MODEL_RESIDUAL;
            }
            else if(request.torque_contract_valid &&
                    request.torque_contract.valid)
            {
                candidate = &request.torque_contract.candidate;
                commit_reason = candidate->reason;
                (void)ecu_torque_clamp_commit_reverify(
                    candidate, &verification,
                    &committed_torque_nm, &commit_reason);
            }

            const int16_t committed_0p1nm =
                torque_nm_to_0p1nm(committed_torque_nm);
            if(committed_0p1nm == 0)
            {
                committed_torque_nm = 0.0f;
                ecu_cm200_build_disable_packet(request.packet.data);
            }
            else
            {
                /* The clamp output is already command-quantized. */
                committed_torque_nm = (float)committed_0p1nm * 0.1f;
                ecu_cm200_build_torque_packet(request.packet.data,
                                              committed_0p1nm);
            }

            ecu_cm200_apply_rolling_counter(request.packet.data,
                                            transmitted_counter);
            tx_status = canbus_transmit_ready(canbus, &request.packet);
            if(tx_status == HAL_OK)
            {
                cm200_note_command_tx(&data->board.cm200,
                                      transmitted_counter,
                                      committed_0p1nm != 0,
                                      committed_0p1nm,
                                      commit_now_ms);

                const bool steady_current_consistent =
                    data->current_residual_monitor.latest_sample_valid &&
                    data->current_residual_monitor.latest_sample_within_envelope &&
                    !data->current_residual_monitor.fault_latched;
                ecu_torque_clamp_note_hardware_commit(
                    &data->torque_clamp_state,
                    candidate,
                    committed_torque_nm,
                    commit_reason,
                    commit_now_ms * 1000u,
                    physical_zero_confirmed_locked(data),
                    steady_current_consistent,
                    &data->pack_current_calibration_runtime);

                data->cm200_command_torque_0p1nm = committed_0p1nm;
                data->torque_clamp_reason = (uint8_t)commit_reason;
                if(candidate != NULL)
                {
                    data->battery_authority_state =
                        (committed_torque_nm == candidate->selected_torque_nm) ?
                        (uint8_t)candidate->battery_authority_state :
                        (uint8_t)ECU_BATTERY_AUTHORITY_TORQUE_EXHAUSTED;
                }
                publish_prediction_locked(data, candidate,
                                          committed_torque_nm,
                                          commit_now_ms * 1000u);
            }
            taskEXIT_CRITICAL();

            if(tx_status == HAL_OK)
            {
                data->cm200_rolling_counter =
                    ecu_cm200_next_rolling_counter(transmitted_counter);
            }
        }
        else
        {
            tx_status = canbus_transmit_ready(canbus, &request.packet);
        }

        if(tx_status != HAL_OK)
        {
            data->canbus_tx_fault = true;
            update_current_residual_monitor(data, HAL_GetTick());
            continue;
        }

        data->canbus_tx_fault = false;
        if(HAL_CAN_GetState(canbus->hcan) == HAL_CAN_STATE_LISTENING)
        {
            if(data->canbus_hw_fault)
            {
                data->can_recovery_count++;
            }
            (void)HAL_CAN_ResetError(canbus->hcan);
            data->canbus_hw_fault = false;
            data->can_error_code = HAL_CAN_ERROR_NONE;
        }

        update_current_residual_monitor(data, HAL_GetTick());
    }
}
