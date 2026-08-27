/**
 * @file apps_task.c
 * @author Cole Bardin (cab572@drexel.edu)
 * @brief APPS acquisition and 100 Hz torque-candidate generation.
 *
 * The APPS task performs the bounded model/search work and publishes a
 * torque-command contract.  It does not claim that the command was applied.
 * The CAN task performs the final comparison-only re-verification after any
 * mailbox delay and updates clamp state only after the CM200 packet is
 * accepted into a hardware transmit mailbox.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "tasks/apps_task.h"
#include "ext_drivers/canbus.h"
#include "ext_drivers/cm200.h"
#include "ext_drivers/ecu_safety.h"
#include "ext_drivers/elapsed_fault_timer.h"
#include "power/ecu_pack_current_calibration.h"
#include "power/ecu_torque_clamp.h"

void apps_task_fn(void *arg);

static StaticTask_t apps_task_tcb;
static StackType_t apps_task_stack[ECU_STACK_APPS_WORDS];
static TaskHandle_t apps_task_handle = NULL;

static uint32_t age_ms_to_us(uint32_t age_ms)
{
    return (age_ms > (UINT32_MAX / 1000u)) ? UINT32_MAX : (age_ms * 1000u);
}

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

TaskHandle_t apps_task_start(app_data_t *data)
{
    if(data == NULL)
    {
        return NULL;
    }

    if(apps_task_handle == NULL)
    {
        apps_task_handle = xTaskCreateStatic(apps_task_fn,
            "APPS task", ECU_STACK_APPS_WORDS, (void *)data, APPS_PRIO,
            apps_task_stack, &apps_task_tcb);
    }
    return apps_task_handle;
}

void apps_task_fn(void *arg)
{
    app_data_t *data = (app_data_t *)arg;
    if(data == NULL)
    {
        vTaskDelete(NULL);
        return;
    }

    poten_t *apps1 = &data->board.apps1;
    poten_t *apps2 = &data->board.apps2;
    int16_t slew_torque_0p1nm = 0;
    uint8_t disable_unlock_cycles = ECU_CM200_DISABLE_UNLOCK_CYCLES;
    ecu_elapsed_fault_timer_t plausibility_timer = {0};

    for(;;)
    {
        const uint32_t entry = osKernelGetTickCount();
        data->apps_heartbeat_tick = entry;

        const bool adc_ok =
            (stm32f767_adc_read_checked(apps1->handle, &apps1->count) == HAL_OK) &&
            (stm32f767_adc_read_checked(apps2->handle, &apps2->count) == HAL_OK);

        const float apps1_raw_percent = poten_get_raw_percent(apps1);
        const float apps2_raw_percent = poten_get_raw_percent(apps2);
        apps1->percent = poten_get_percent(apps1);
        apps2->percent = poten_get_percent(apps2);

        float throttle_raw = 100.0f -
            ((apps1->percent + apps2->percent) / 2.0f);
        if(throttle_raw < 0.0f)
        {
            throttle_raw = 0.0f;
        }
        data->throttle = (int)throttle_raw;

        /* T.4.2.5 (2022). Plausibility uses the current samples so the moving
         * average cannot conceal a sensor split for the permitted interval. */
        const bool apps_range_ok = adc_ok &&
            poten_check_failure(apps1->count,
                                APPS_IMPLAUSIBILITY_MAX,
                                APPS_IMPLAUSIBILITY_MIN) &&
            poten_check_failure(apps2->count,
                                APPS_IMPLAUSIBILITY_MAX,
                                APPS_IMPLAUSIBILITY_MIN);
        const bool apps_split =
            fabsf(apps1_raw_percent - apps2_raw_percent) >
            (float)PLAUSIBILITY_THRESH;
        const bool apps_split_timed_out = ecu_elapsed_fault_timer_update(
            &plausibility_timer, apps_split, entry,
            ECU_APPS_IMPLAUSIBILITY_LIMIT_MS);
        data->apps_fault = !apps_range_ok || apps_split_timed_out;

        const ecu_torque_inputs_t torque_inputs = {
            .cascadia_ok = data->cascadia_ok,
            .hard_fault = data->hard_fault || data->current_model_residual_fault,
            .apps_fault = data->apps_fault,
            .bppc_fault = data->bppc_fault,
            .bse_fault = data->bse_fault,
            .ams_fault = data->ams_fault,
            .canbus_fault = (data->canbus_fault || data->canbus_hw_fault),
            .canbus_rx_fault = data->canbus_rx_fault,
            .canbus_tx_fault = data->canbus_tx_fault,
            .imd_fail = data->imd_fail,
            .bms_fail = data->bms_fail,
            .bspd_fail = data->bspd_fail,
            .cm200_fault = (data->cm200_fault || !data->cm200_ready),
            .rtd_mode = data->rtd_mode,
        };

        const bool torque_allowed = ecu_torque_allowed(&torque_inputs);
        canbus_tx_request_t tx_request;
        memset(&tx_request, 0, sizeof(tx_request));
        tx_request.packet.id = CM_CANBUS_ID;
        ecu_cm200_build_disable_packet(tx_request.packet.data);

        if(!ECU_OUTPUTS_INHIBITED &&
           ecu_cm200_update_unlock(torque_allowed, &disable_unlock_cycles))
        {
            const int16_t target_torque_0p1nm =
                (int16_t)((data->throttle * MAXTRQ) / 10);

            der26_power_immediate_authority_t authority;
            ecu_torque_clamp_state_t state_snapshot;
            int16_t speed_rpm;
            int16_t vdc_0p1v;
            int16_t inv_temp_0p1c;
            int16_t motor_temp_0p1c;
            int16_t torque_feedback_0p1nm;
            int16_t positive_cap_0p1nm;
            int16_t negative_cap_0p1nm;
            uint32_t speed_generation;
            uint32_t vdc_generation;
            uint32_t temperature_generation;
            uint32_t capability_generation;
            uint32_t speed_age_ms;
            uint32_t vdc_age_ms;
            uint32_t temperature_age_ms;
            bool authority_valid;
            bool inverter_enabled;
            bool cm200_ready;
            bool internal_state_fresh;
            bool torque_feedback_fresh;

            taskENTER_CRITICAL();
            const int16_t capability_limited_0p1nm =
                cm200_clamp_motoring_torque(&data->board.cm200,
                                            target_torque_0p1nm);
            authority = data->board.ams.power_authority;
            authority_valid = data->board.ams.power_authority_valid;
            speed_rpm = data->board.cm200.motor_speed_rpm;
            vdc_0p1v = data->board.cm200.dc_bus_voltage_0p1v;
            inv_temp_0p1c = data->board.cm200.inverter_hotspot_temp_0p1c;
            motor_temp_0p1c = data->board.cm200.motor_temp_0p1c;
            torque_feedback_0p1nm = data->board.cm200.torque_feedback_0p1nm;
            positive_cap_0p1nm =
                data->board.cm200.motor_torque_available_0p1nm;
            negative_cap_0p1nm =
                data->board.cm200.regen_torque_available_0p1nm;
            speed_generation =
                data->board.cm200.frame[CM200_FRAME_MOTOR_POSITION].rx_count;
            vdc_generation =
                data->board.cm200.frame[CM200_FRAME_VOLTAGE].rx_count;
            temperature_generation =
                data->board.cm200.frame[CM200_FRAME_TEMPERATURES_3].rx_count;
            capability_generation =
                data->board.cm200.frame[CM200_FRAME_TORQUE_CAPABILITY].rx_count;
            speed_age_ms = cm200_frame_age_ms(&data->board.cm200,
                                               CM200_FRAME_MOTOR_POSITION,
                                               entry);
            vdc_age_ms = cm200_frame_age_ms(&data->board.cm200,
                                             CM200_FRAME_VOLTAGE,
                                             entry);
            temperature_age_ms = cm200_frame_age_ms(
                &data->board.cm200, CM200_FRAME_TEMPERATURES_3, entry);
            inverter_enabled = data->board.cm200.inverter_enabled;
            cm200_ready = data->cm200_ready;
            internal_state_fresh =
                data->board.cm200.frame[CM200_FRAME_INTERNAL_STATES].valid &&
                data->board.cm200.frame[CM200_FRAME_INTERNAL_STATES].sane &&
                !data->board.cm200.frame[CM200_FRAME_INTERNAL_STATES].stale;
            torque_feedback_fresh =
                data->board.cm200.frame[CM200_FRAME_TORQUE_TIMER].valid &&
                data->board.cm200.frame[CM200_FRAME_TORQUE_TIMER].sane &&
                !data->board.cm200.frame[CM200_FRAME_TORQUE_TIMER].stale;
            state_snapshot = data->torque_clamp_state;
            taskEXIT_CRITICAL();

            data->cm200_target_torque_0p1nm = capability_limited_0p1nm;

            /* Comfort slew is applied before the safety clamp. Any clamp
             * reduction or zero can bypass the comfort trajectory later. */
            slew_torque_0p1nm = ecu_torque_slew_limit(
                slew_torque_0p1nm,
                capability_limited_0p1nm,
                ECU_TORQUE_RISE_STEP_0P1NM,
                ECU_TORQUE_FALL_STEP_0P1NM);

            const float zero_confirm_nm =
                g_ecu_pack_current_calibration.zero_enter_nm;
            const bool physical_zero_confirmed =
                (internal_state_fresh && !inverter_enabled) ||
                (torque_feedback_fresh &&
                 (abs(torque_feedback_0p1nm) <=
                  (int16_t)(zero_confirm_nm * 10.0f)));

            const ecu_torque_clamp_input_t clamp_input = {
                .requested_torque_nm = (float)slew_torque_0p1nm * 0.1f,
                .motor_speed_rpm = (float)speed_rpm,
                .dc_bus_voltage_v = (float)vdc_0p1v * 0.1f,
                .inverter_temp_c = (float)inv_temp_0p1c * 0.1f,
                .motor_temp_c = (float)motor_temp_0p1c * 0.1f,
                .cm200_positive_cap_nm = (float)positive_cap_0p1nm * 0.1f,
                .cm200_negative_cap_nm = (float)negative_cap_0p1nm * 0.1f,
                .dcl_a = authority.discharge.current_limit_a,
                .ccl_a = authority.charge_regen.current_limit_a,
                .now_us = entry * 1000u,
                .motor_speed_age_us = age_ms_to_us(speed_age_ms),
                .dc_bus_voltage_age_us = age_ms_to_us(vdc_age_ms),
                .inverter_temp_age_us = age_ms_to_us(temperature_age_ms),
                .motor_temp_age_us = age_ms_to_us(temperature_age_ms),
                .speed_generation = speed_generation,
                .vdc_generation = vdc_generation,
                .temperature_generation = temperature_generation,
                .capability_generation = capability_generation,
                .authority_received_ms = authority.received_ms,
                .calibration_generation =
                    data->pack_current_calibration_runtime.generation,
                .authority_counter = authority.counter,
                .discharge_authorized =
                    authority.discharge.authorized != 0u,
                .charge_authorized =
                    authority.charge_regen.authorized != 0u,
                .authority_valid = authority_valid,
                .operating_point_valid = cm200_ready && (vdc_0p1v > 0),
                .physical_zero_confirmed = physical_zero_confirmed,
            };

            ecu_torque_clamp_output_t clamp_output;
            const bool dwt_available = stm32f767_cycle_counter_available();
            const uint32_t clamp_start_cycles =
                stm32f767_cycle_counter_read();
            const bool clamp_ok = ecu_torque_clamp_run(
                &clamp_input,
                &data->pack_current_calibration_runtime,
                &state_snapshot,
                &clamp_output);
            const uint32_t clamp_elapsed_cycles = dwt_available ?
                (uint32_t)(stm32f767_cycle_counter_read() -
                           clamp_start_cycles) : 0u;
            const uint32_t clamp_elapsed_us =
                stm32f767_cycles_to_us(clamp_elapsed_cycles);
            const uint32_t clamp_done = osKernelGetTickCount();
            const bool clamp_tick_overrun =
                ((uint32_t)(clamp_done - entry) >=
                 (ECU_CLAMP_CONTROL_PERIOD_US / 1000u));
            const bool clamp_deadline_overrun =
                clamp_tick_overrun ||
                (dwt_available &&
                 (clamp_elapsed_us >= ECU_TORQUE_CLAMP_HARD_BUDGET_US));

            data->torque_clamp_last_cycles = clamp_elapsed_cycles;
            if(clamp_elapsed_cycles > data->torque_clamp_max_cycles)
            {
                data->torque_clamp_max_cycles = clamp_elapsed_cycles;
            }
            if(dwt_available &&
               (clamp_elapsed_us >= ECU_TORQUE_CLAMP_SOFT_BUDGET_US))
            {
                if(data->torque_clamp_soft_overrun_count != UINT32_MAX)
                {
                    data->torque_clamp_soft_overrun_count++;
                }
            }
            if(clamp_deadline_overrun)
            {
                if(data->torque_clamp_deadline_overrun_count != UINT32_MAX)
                {
                    data->torque_clamp_deadline_overrun_count++;
                }
                if(data->torque_clamp_consecutive_overruns != UINT32_MAX)
                {
                    data->torque_clamp_consecutive_overruns++;
                }

                /* A hard timing miss cannot commit nonzero torque. Two
                 * consecutive misses latch a supervisor-visible safety fault
                 * and deassert Firmware_OK through the error task. */
                if(clamp_ok && clamp_output.output_valid)
                {
                    ecu_torque_clamp_force_static_zero(
                        &clamp_input,
                        &data->pack_current_calibration_runtime,
                        &state_snapshot,
                        ECU_CLAMP_REASON_DEADLINE_OVERRUN,
                        &clamp_output);
                }
                if(data->torque_clamp_consecutive_overruns >=
                   ECU_TORQUE_CLAMP_OVERRUN_TRIP_COUNT)
                {
                    data->torque_clamp_overrun_fault = true;
                }
            }
            else
            {
                data->torque_clamp_consecutive_overruns = 0u;
            }

            data->torque_clamp_steady_calls = clamp_output.steady_model_calls;
            data->torque_clamp_transition_calls =
                clamp_output.transition_model_calls;
            data->torque_clamp_cells_evaluated =
                clamp_output.torque_cells_evaluated;
            data->torque_clamp_output_valid =
                clamp_ok && clamp_output.output_valid;
            data->battery_authority_state =
                (uint8_t)clamp_output.battery_authority_state;

            if(clamp_ok && clamp_output.output_valid)
            {
                tx_request.torque_contract.candidate = clamp_output;
                tx_request.torque_contract.input_snapshot = clamp_input;
                tx_request.torque_contract.valid = true;
                tx_request.torque_contract_valid = true;

                const int16_t selected_0p1nm =
                    torque_nm_to_0p1nm(clamp_output.selected_torque_nm);
                if(selected_0p1nm == 0)
                {
                    /* Safety zero does not wait for comfort-slew decay. */
                    slew_torque_0p1nm = 0;
                    ecu_cm200_build_disable_packet(tx_request.packet.data);
                }
                else
                {
                    ecu_cm200_build_torque_packet(tx_request.packet.data,
                                                  selected_0p1nm);
                }
            }
            else
            {
                slew_torque_0p1nm = 0;
                data->torque_clamp_reason =
                    (uint8_t)ECU_CLAMP_REASON_CALIBRATION_INVALID;
                ecu_cm200_build_disable_packet(tx_request.packet.data);
            }
        }
        else
        {
            /* Fault and inhibit transitions bypass normal comfort slew. */
            slew_torque_0p1nm = 0;
            data->cm200_target_torque_0p1nm = 0;
            data->torque_clamp_output_valid = false;
            if(data->current_model_residual_fault)
            {
                data->torque_clamp_reason =
                    (uint8_t)ECU_CLAMP_REASON_CURRENT_MODEL_RESIDUAL;
            }
            ecu_cm200_build_disable_packet(tx_request.packet.data);
        }

        /* Latest-value mailbox: a newer disable replaces any older unsent
         * positive torque. Actual committed state is updated by CAN task only. */
        if(canbus_queue_tx(&data->board.canbus, &tx_request) != HAL_OK)
        {
            data->canbus_tx_fault = true;
        }

        osDelayUntil(entry + (1000u / APPS_FREQ));
    }
}
