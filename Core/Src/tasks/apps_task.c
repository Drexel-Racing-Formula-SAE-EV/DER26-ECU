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
    uint16_t throttle_hex;
    uint32_t entry;
    uint8_t disable_unlock_cycles = ECU_CM200_DISABLE_UNLOCK_CYCLES;
    bool torque_allowed;

    tx_packet.id = CM_CANBUS_ID;
    ecu_cm200_build_disable_packet(tx_packet.data);

    for(;;)
    {
        entry = osKernelGetTickCount();

        apps1->count = stm32f767_adc_read(apps1->handle);
        apps2->count = stm32f767_adc_read(apps2->handle);

        apps1->percent = poten_get_percent(apps1);
        apps2->percent = poten_get_percent(apps2);

        throttle_raw = 100.0f - ((apps1->percent + apps2->percent) / 2.0f);
        if(throttle_raw < 0.0f)
        {
            throttle_raw = 0.0f;
        }
        data->throttle = (int)throttle_raw;

        /* T.4.2.5 (2022). */
        if(!poten_check_plausibility(apps1->percent, apps2->percent, PLAUSIBILITY_THRESH, APPS_FREQ / 10))
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
            .canbus_fault = data->canbus_fault,
            .canbus_rx_fault = data->canbus_rx_fault,
            .canbus_tx_fault = data->canbus_tx_fault,
            .imd_fail = data->imd_fail,
            .bms_fail = data->bms_fail,
            .bspd_fail = data->bspd_fail,
            .rtd_mode = data->rtd_mode,
        };

        torque_allowed = ecu_torque_allowed(&torque_inputs);
        tx_packet.id = CM_CANBUS_ID;

        if(ecu_cm200_update_unlock(torque_allowed, &disable_unlock_cycles))
        {
            throttle_hex = (uint16_t)(data->throttle * MAXTRQ / 10.0f); /* CM CANBus protocol scale. */
            ecu_cm200_build_torque_packet(tx_packet.data, throttle_hex);
        }
        else
        {
            ecu_cm200_build_disable_packet(tx_packet.data);
        }

        if(canbus_queue_tx(&data->board.canbus, &tx_packet) == HAL_OK)
        {
            if(data->canbus_task != NULL)
            {
                xTaskNotify(data->canbus_task, CANBUS_APPS, eSetBits);
            }
            else
            {
                data->canbus_tx_fault = true;
            }
        }
        else
        {
            data->canbus_tx_fault = true;
        }

        osDelayUntil(entry + (1000 / APPS_FREQ));
    }
}
