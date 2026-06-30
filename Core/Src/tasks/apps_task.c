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

static inline uint8_t u16_lsb(uint16_t value)
{
	return (uint8_t)(value & 0x00ffu);
}

static inline uint8_t u16_msb(uint16_t value)
{
	return (uint8_t)((value >> 8u) & 0x00ffu);
}

/**
 * @brief Actual APPS task function
 *
 * @param arg App_data struct pointer converted to void pointer
 */
static void apps_task_fn(void *arg);

TaskHandle_t apps_task_start(app_data_t *data)
{
    TaskHandle_t handle = NULL;

    if(data == NULL)
    {
        return NULL;
    }

    xTaskCreate(apps_task_fn, "APPS task", 128, (void *)data, APPS_PRIO, &handle);
    return handle;
}

static void apps_task_fn(void *arg)
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

    tx_packet.id = CM_CANBUS_ID;
    memset(tx_packet.data, 0, sizeof(tx_packet.data));

    for(;;)
    {
        entry = osKernelGetTickCount();

        apps1->count = stm32f767_adc_read(apps1->handle);
        apps2->count = stm32f767_adc_read(apps2->handle);


//        if(!poten_check_failure(apps1->count, APPS_IMPLAUSIBILITY_MAX, APPS_IMPLAUSIBILITY_MIN) ||
//           !poten_check_failure(apps2->count, APPS_IMPLAUSIBILITY_MAX, APPS_IMPLAUSIBILITY_MIN))
//        {
//        	data->apps_fault = true;
//        } else {
//
//        }

        apps1->percent = poten_get_percent(apps1);
        apps2->percent = poten_get_percent(apps2);

        throttle_raw = 100.0f - ((apps1->percent + apps2->percent) / 2.0f);
//        throttle_raw = 100 - apps2->percent;
        if(throttle_raw < 0){
        	throttle_raw = 0;
        }
//        if(throttle_raw > 15){
//        	throttle_raw = 15;
//        }
        data->throttle = (int)throttle_raw;

        // T.4.2.5 (2022)
        if(!poten_check_plausibility(apps1->percent, apps2->percent, PLAUSIBILITY_THRESH, APPS_FREQ / 10))
        {
            data->apps_fault = true;
        } else if(!poten_check_failure(apps1->count, APPS_IMPLAUSIBILITY_MAX, APPS_IMPLAUSIBILITY_MIN) ||
                  !poten_check_failure(apps2->count, APPS_IMPLAUSIBILITY_MAX, APPS_IMPLAUSIBILITY_MIN)) {
        	data->apps_fault = true;
        } else {
        	data->apps_fault = false;
        }

        if(!data->cascadia_ok)
        {
            memset(tx_packet.data, 0, sizeof(tx_packet.data));
        }
        else if(data->hard_fault ||
                data->apps_fault ||
                data->rtd_mode != RTD_ENABLED ||
                data->bppc_fault ||
                data->bse_fault ||
                data->ams_fault ||
                data->canbus_fault ||
                data->canbus_rx_fault ||
                data->canbus_tx_fault ||
                data->imd_fail ||
                data->bms_fail ||
                data->bspd_fail)
        {
            tx_packet.data[0] = 0;
            tx_packet.data[1] = 0;
            tx_packet.data[2] = 0;
            tx_packet.data[3] = 0;
            tx_packet.data[4] = 0; // Direction: 0-backward, 1-forward (motor is mounted backwards
            tx_packet.data[5] = 0; // Inverter Enable: 0-disable, 1-enable
            tx_packet.data[6] = 0;
            tx_packet.data[7] = 0;
        }
        else
        {
            throttle_hex = (uint16_t)((float)(data->throttle * MAXTRQ) / 10.0f); // CM CANBus Protocol
            tx_packet.data[0] = u16_lsb(throttle_hex);
            tx_packet.data[1] = u16_msb(throttle_hex);
            tx_packet.data[2] = 0;
            tx_packet.data[3] = 0;
            tx_packet.data[4] = 1; // Direction: 0-backward, 1-forward (motor is mounted backwards
            tx_packet.data[5] = 1; // Inverter Enable: 0-disable, 1-enable
            tx_packet.data[6] = 0;
            tx_packet.data[7] = 0;
        }
        taskENTER_CRITICAL();
        data->board.canbus.tx_packet = tx_packet;
        taskEXIT_CRITICAL();

        if(data->canbus_task != NULL)
        {
            xTaskNotify(data->canbus_task, CANBUS_APPS, eSetBits);
        }
        else
        {
            data->canbus_tx_fault = true;
        }
        osDelayUntil(entry + (1000 / APPS_FREQ));
    }
}
