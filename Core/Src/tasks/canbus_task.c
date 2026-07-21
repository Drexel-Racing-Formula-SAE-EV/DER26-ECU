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
#include "ext_drivers/ecu_safety.h"

/**
 * @brief CANBus task function
 *
 * @param arg App_data struct pointer converted to void pointer
 */
void canbus_task_fn(void *arg);

TaskHandle_t canbus_task_start(app_data_t *data) {
    TaskHandle_t handle = NULL;

    if(data == NULL)
    {
        return NULL;
    }

    xTaskCreate(canbus_task_fn, "CANBus Task", 512, (void *)data, CAN_PRIO, &handle);
    return handle;
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
    canbus_packet_t can_packet;
    HAL_StatusTypeDef tx_status;

    for(;;)
    {
        data->can_heartbeat_tick = osKernelGetTickCount();
        if(canbus->tx_queue == NULL)
        {
            data->canbus_tx_fault = true;
            osDelay(10u);
            continue;
        }

        if(xQueueReceive(canbus->tx_queue, &can_packet, portMAX_DELAY) != pdPASS)
        {
            data->canbus_tx_fault = true;
            continue;
        }

        do
        {
            if(can_packet.id == CM_CANBUS_ID)
            {
                ecu_cm200_apply_rolling_counter(can_packet.data, data->cm200_rolling_counter);
            }
            tx_status = canbus_transmit(canbus, &can_packet, CANBUS_TX_TIMEOUT_MS);
            if(tx_status != HAL_OK)
            {
                data->canbus_tx_fault = true;
                break;
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
            if(can_packet.id == CM_CANBUS_ID)
            {
                data->cm200_rolling_counter = ecu_cm200_next_rolling_counter(data->cm200_rolling_counter);
            }
        } while(xQueueReceive(canbus->tx_queue, &can_packet, 0u) == pdPASS);
    }
}
