/**
 * @file canbus_task.c
 * @author Cole Bardin (cab572@drexel.edu)
 * @brief
 * @version 0.1
 * @date 2023-04-24
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <string.h>

#include "tasks/canbus_task.h"
#include "ext_drivers/canbus.h"

/**
 * @brief CANBus task function
 *
 * @param arg App_data struct pointer converted to void pointer
 */
static void canbus_task_fn(void *arg);

TaskHandle_t canbus_task_start(app_data_t *data) {
    TaskHandle_t handle = NULL;

    if(data == NULL)
    {
        return NULL;
    }

    xTaskCreate(canbus_task_fn, "CANBus Task", 512, (void *)data, CAN_PRIO, &handle);
    return handle;
}

static void canbus_task_fn(void *arg)
{
    app_data_t *data = (app_data_t *)arg;

    if(data == NULL)
    {
        vTaskDelete(NULL);
        return;
    }

    canbus_t *canbus = &data->board.canbus;
    canbus_packet_t can_packet;
    uint32_t task_notification;

    for(;;)
    {
        xTaskNotifyWait(0, 0xFFFFFFFFu, &task_notification, HAL_MAX_DELAY);
        if(task_notification & CANBUS_APPS)
        {
            taskENTER_CRITICAL();
            can_packet = canbus->tx_packet;
            memset(canbus->tx_packet.data, 0, sizeof(canbus->tx_packet.data));
            taskEXIT_CRITICAL();

            data->canbus_tx_fault = (canbus_transmit(canbus, &can_packet, CANBUS_TX_TIMEOUT_MS) != HAL_OK);
        }
    }
}
