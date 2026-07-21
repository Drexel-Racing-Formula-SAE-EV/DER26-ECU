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
#include "ext_drivers/cm200.h"
#include "ext_drivers/ecu_safety.h"

#define CANBUS_COMMAND_WAIT_MS 25u

/**
 * @brief CANBus task function
 *
 * @param arg App_data struct pointer converted to void pointer
 */
void canbus_task_fn(void *arg);

static bool canbus_torque_still_allowed(app_data_t *data)
{
    bool protocol_ready;
    const ecu_torque_inputs_t inputs = {
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
        .cm200_fault = (data->cm200_fault || !data->cm200_ready),
        .rtd_mode = data->rtd_mode,
    };

    taskENTER_CRITICAL();
    protocol_ready = (ams_allows_torque(&data->board.ams) &&
                      cm200_allows_torque(&data->board.cm200));
    taskEXIT_CRITICAL();

    return (!ECU_OUTPUTS_INHIBITED &&
            protocol_ready &&
            ecu_torque_allowed(&inputs));
}

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

        if(xQueueReceive(canbus->tx_queue,
                         &can_packet,
                         pdMS_TO_TICKS(CANBUS_COMMAND_WAIT_MS)) != pdPASS)
        {
            /* An APPS task stall must not leave the last enable/torque command
             * as the final value transmitted. */
            can_packet.id = CM_CANBUS_ID;
            ecu_cm200_build_disable_packet(can_packet.data);
        }

        if(can_packet.id == CM_CANBUS_ID)
        {
            if(ecu_cm200_packet_enabled(can_packet.data) &&
               !canbus_torque_still_allowed(data))
            {
                ecu_cm200_build_disable_packet(can_packet.data);
            }
            ecu_cm200_apply_rolling_counter(can_packet.data, data->cm200_rolling_counter);
        }
        tx_status = canbus_transmit(canbus, &can_packet, CANBUS_TX_TIMEOUT_MS);
        if(tx_status != HAL_OK)
        {
            data->canbus_tx_fault = true;
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
        if(can_packet.id == CM_CANBUS_ID)
        {
            const uint8_t transmitted_counter = data->cm200_rolling_counter;
            taskENTER_CRITICAL();
            cm200_note_command_tx(&data->board.cm200,
                                  transmitted_counter,
                                  ecu_cm200_packet_enabled(can_packet.data),
                                  ecu_cm200_packet_torque(can_packet.data),
                                  HAL_GetTick());
            taskEXIT_CRITICAL();
            data->cm200_rolling_counter = ecu_cm200_next_rolling_counter(transmitted_counter);
        }
    }
}
