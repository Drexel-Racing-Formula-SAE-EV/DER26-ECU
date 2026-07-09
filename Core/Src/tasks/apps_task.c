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

#define TO_LSB(x) ((uint8_t)((x) & 0xffu))
#define TO_MSB(x) ((uint8_t)(((x) >> 8u) & 0xffu))
#define CM200_DISABLE_UNLOCK_CYCLES 5u

/**
 * @brief Actual APPS task function
 *
 * @param arg App_data struct pointer converted to void pointer
 */
void apps_task_fn(void *arg);

static void cm200_build_disable_packet(canbus_packet_t *packet)
{
    if(packet == NULL)
    {
        return;
    }

    packet->id = CM_CANBUS_ID;
    memset(packet->data, 0, sizeof(packet->data));
    packet->data[4] = 0u; /* Direction field. Keep disabled while zero torque. */
    packet->data[5] = 0u; /* Inverter enable bit = disabled. */
}

static void cm200_build_torque_packet(canbus_packet_t *packet, uint16_t torque_cmd)
{
    if(packet == NULL)
    {
        return;
    }

    packet->id = CM_CANBUS_ID;
    memset(packet->data, 0, sizeof(packet->data));
    packet->data[0] = TO_LSB(torque_cmd);
    packet->data[1] = TO_MSB(torque_cmd);
    packet->data[2] = 0u;
    packet->data[3] = 0u;
    packet->data[4] = 1u; /* Direction: 0-backward, 1-forward; motor is mounted backwards. */
    packet->data[5] = 1u; /* Inverter Enable: 0-disable, 1-enable. */
    packet->data[6] = 0u;
    packet->data[7] = 0u;
}

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
    uint8_t disable_unlock_cycles = CM200_DISABLE_UNLOCK_CYCLES;
    bool torque_allowed;

    cm200_build_disable_packet(&tx_packet);

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

        torque_allowed = (data->cascadia_ok &&
                          !data->hard_fault &&
                          !data->apps_fault &&
                          (data->rtd_mode == RTD_ENABLED) &&
                          !data->bppc_fault &&
                          !data->bse_fault &&
                          !data->ams_fault &&
                          !data->canbus_fault &&
                          !data->canbus_rx_fault &&
                          !data->canbus_tx_fault &&
                          !data->imd_fail &&
                          !data->bms_fail &&
                          !data->bspd_fail);

        if(!torque_allowed)
        {
            disable_unlock_cycles = CM200_DISABLE_UNLOCK_CYCLES;
            cm200_build_disable_packet(&tx_packet);
        }
        else if(disable_unlock_cycles > 0u)
        {
            /*
             * CM200 enable lockout requires disable commands before enable after boot/fault.
             * Keep zero torque/disable for several APPS cycles before enabling torque.
             */
            disable_unlock_cycles--;
            cm200_build_disable_packet(&tx_packet);
        }
        else
        {
            throttle_hex = (uint16_t)(data->throttle * MAXTRQ / 10.0f); /* CM CANBus protocol scale. */
            cm200_build_torque_packet(&tx_packet, throttle_hex);
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
