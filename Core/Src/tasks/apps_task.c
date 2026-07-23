/**
 * @file apps_task.c
 * @author Cole Bardin (cab572@drexel.edu)
 * @brief
 * @version 0.1
 * @date 2023-04-17
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <math.h>
#include <string.h>

#include "main.h"
#include "tasks/apps_task.h"
#include "ext_drivers/canbus.h"
#include "ext_drivers/cm200.h"
#include "ext_drivers/ecu_safety.h"

/**
 * @brief Actual APPS task function
 *
 * @param arg App_data struct pointer converted to void pointer
 */
void apps_task_fn(void *arg);

TaskHandle_t apps_task_start(app_data_t *data)
{
    TaskHandle_t handle = NULL;

    if(data == NULL)
    {
        return NULL;
    }

    xTaskCreate(apps_task_fn, "APPS task", 256, (void *)data, APPS_PRIO, &handle);
    return handle;
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
    canbus_packet_t tx_packet;

    float throttle_raw;
    float apps1_raw_percent;
    float apps2_raw_percent;
    int16_t torque_0p1nm;
    int16_t target_torque_0p1nm;
    int16_t slew_torque_0p1nm = 0;
    uint32_t entry;
    uint8_t disable_unlock_cycles = ECU_CM200_DISABLE_UNLOCK_CYCLES;
    bool torque_allowed;
    bool adc_ok;

    tx_packet.id = CM_CANBUS_ID;
    ecu_cm200_build_disable_packet(tx_packet.data);

    for(;;)
    {
        entry = osKernelGetTickCount();
        data->apps_heartbeat_tick = entry;

        adc_ok = ((stm32f767_adc_read_checked(apps1->handle, &apps1->count) == HAL_OK) &&
                  (stm32f767_adc_read_checked(apps2->handle, &apps2->count) == HAL_OK));

        apps1_raw_percent = poten_get_raw_percent(apps1);
        apps2_raw_percent = poten_get_raw_percent(apps2);
        apps1->percent = poten_get_percent(apps1);
        apps2->percent = poten_get_percent(apps2);

        throttle_raw = 100.0f - ((apps1->percent + apps2->percent) / 2.0f);
        if(throttle_raw < 0.0f)
        {
            throttle_raw = 0.0f;
        }
        data->throttle = (int)throttle_raw;

        /* T.4.2.5 (2022). */
        /* Plausibility is evaluated on the current samples, not the moving
         * average, so the filter cannot conceal a sensor split for 500 ms. */
        if(!adc_ok)
        {
            data->apps_fault = true;
        }
        else if(!poten_check_plausibility(apps1_raw_percent, apps2_raw_percent, PLAUSIBILITY_THRESH, APPS_FREQ / 10))
        {
            data->apps_fault = true;
        }
        else if(!poten_check_failure(apps1->count, APPS_IMPLAUSIBILITY_MAX, APPS_IMPLAUSIBILITY_MIN) ||
                !poten_check_failure(apps2->count, APPS_IMPLAUSIBILITY_MAX, APPS_IMPLAUSIBILITY_MIN))
        {
            data->apps_fault = true;
        }
        else
        {
            data->apps_fault = false;
        }

        const ecu_torque_inputs_t torque_inputs = {
            .cascadia_ok = data->cascadia_ok,
            .hard_fault = data->hard_fault,
            .apps_fault = data->apps_fault,
            .bppc_fault = data->bppc_fault,
            .bse_fault = data->bse_fault,
            .ams_fault = data->ams_fault,
            /* Include the ISR-owned hardware fault directly.  The lower-rate
             * aggregate canbus_fault may not have observed it yet, and the CAN
             * task can clear a recovered transient after a successful send. */
            .canbus_fault = (data->canbus_fault || data->canbus_hw_fault),
            .canbus_rx_fault = data->canbus_rx_fault,
            .canbus_tx_fault = data->canbus_tx_fault,
            .imd_fail = data->imd_fail,
            .bms_fail = data->bms_fail,
            .bspd_fail = data->bspd_fail,
            .cm200_fault = (data->cm200_fault || !data->cm200_ready),
            .rtd_mode = data->rtd_mode,
        };

        torque_allowed = ecu_torque_allowed(&torque_inputs);
        tx_packet.id = CM_CANBUS_ID;

        if(!ECU_OUTPUTS_INHIBITED && ecu_cm200_update_unlock(torque_allowed, &disable_unlock_cycles))
        {
            target_torque_0p1nm = (int16_t)((data->throttle * MAXTRQ) / 10); /* Nm*10. */
            taskENTER_CRITICAL();
            torque_0p1nm = cm200_clamp_motoring_torque(&data->board.cm200,
                                                        target_torque_0p1nm);
            taskEXIT_CRITICAL();
            data->cm200_target_torque_0p1nm = torque_0p1nm;
            slew_torque_0p1nm = ecu_torque_slew_limit(slew_torque_0p1nm,
                                                       torque_0p1nm,
                                                       ECU_TORQUE_RISE_STEP_0P1NM,
                                                       ECU_TORQUE_FALL_STEP_0P1NM);
            data->cm200_command_torque_0p1nm = slew_torque_0p1nm;
            ecu_cm200_build_torque_packet(tx_packet.data, slew_torque_0p1nm);
        }
        else
        {
            /* Fault and inhibit transitions bypass the normal down-slew. */
            slew_torque_0p1nm = 0;
            data->cm200_target_torque_0p1nm = 0;
            data->cm200_command_torque_0p1nm = 0;
            ecu_cm200_build_disable_packet(tx_packet.data);
        }

        /* The one-slot mailbox wakes the CAN task and replaces any unsent older
         * request, so a disable cannot sit behind stale positive torque. */
        if(canbus_queue_tx(&data->board.canbus, &tx_packet) != HAL_OK)
        {
            data->canbus_tx_fault = true;
        }

        osDelayUntil(entry + (1000 / APPS_FREQ));
    }
}
