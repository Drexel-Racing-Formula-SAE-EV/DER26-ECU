/**
 * @file canbus.c
 * @author Cole Bardin (cab572@drexel.edu)
 * @brief
 * @version 0.1
 * @date 2023-04-24
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "ext_drivers/canbus.h"
#include "ext_drivers/ams.h"
#include "ext_drivers/cm200.h"

#define CANBUS_FILTER_BANK_COUNT 5u
#define CANBUS_IDS_PER_16BIT_LIST_BANK 4u
#define CANBUS_STD_ID_FILTER_VALUE(id) ((uint32_t)((id) << 5u))

static HAL_StatusTypeDef canbus_configure_rx_filters(CAN_HandleTypeDef *hcan)
{
    static const uint16_t allowed_ids[CANBUS_FILTER_BANK_COUNT * CANBUS_IDS_PER_16BIT_LIST_BANK] = {
        AMS_TELEM_CANBUS_ID,
        AMS_ESTIMATOR_CANBUS_ID,
        AMS_ECU_STATUS_CANBUS_ID,
        AMS_ECU_ELECTRICAL_CANBUS_ID,
        AMS_ECU_THERMAL_CANBUS_ID,
        AMS_ECU_HEALTH_CANBUS_ID,
        CM200_TEMPERATURES_1_CAN_ID,
        CM200_TEMPERATURES_2_CAN_ID,
        CM200_TEMPERATURES_3_CAN_ID,
        CM200_MOTOR_POSITION_CAN_ID,
        CM200_CURRENT_CAN_ID,
        CM200_VOLTAGE_CAN_ID,
        CM200_INTERNAL_STATES_CAN_ID,
        CM200_FAULTS_CAN_ID,
        CM200_TORQUE_TIMER_CAN_ID,
        CM200_FIRMWARE_CAN_ID,
        CM200_TORQUE_CAP_CAN_ID,
        /* Duplicate safe entries fill the final three list slots. */
        CM200_FAULTS_CAN_ID,
        CM200_INTERNAL_STATES_CAN_ID,
        AMS_ECU_STATUS_CANBUS_ID,
    };

    if(hcan == NULL)
    {
        return HAL_ERROR;
    }

    for(uint32_t bank = 0u; bank < CANBUS_FILTER_BANK_COUNT; bank++)
    {
        const uint32_t base = bank * CANBUS_IDS_PER_16BIT_LIST_BANK;
        CAN_FilterTypeDef filter = {0};
        filter.FilterBank = bank;
        filter.FilterMode = CAN_FILTERMODE_IDLIST;
        filter.FilterScale = CAN_FILTERSCALE_16BIT;
        filter.FilterIdHigh = CANBUS_STD_ID_FILTER_VALUE(allowed_ids[base + 0u]);
        filter.FilterIdLow = CANBUS_STD_ID_FILTER_VALUE(allowed_ids[base + 1u]);
        filter.FilterMaskIdHigh = CANBUS_STD_ID_FILTER_VALUE(allowed_ids[base + 2u]);
        filter.FilterMaskIdLow = CANBUS_STD_ID_FILTER_VALUE(allowed_ids[base + 3u]);
        filter.FilterFIFOAssignment = CAN_RX_FIFO0;
        filter.FilterActivation = ENABLE;
        filter.SlaveStartFilterBank = 14u;
        if(HAL_CAN_ConfigFilter(hcan, &filter) != HAL_OK)
        {
            return HAL_ERROR;
        }
    }
    return HAL_OK;
}

void canbus_device_init(canbus_t *dev, CAN_HandleTypeDef *hcan, CAN_TxHeaderTypeDef *tx_header)
{
    if((dev == NULL) || (hcan == NULL) || (tx_header == NULL))
    {
        return;
    }

    dev->hcan = hcan;
    dev->tx_header = tx_header;
    dev->tx_dropped_count = 0u;
    dev->tx_replaced_count = 0u;
    dev->rx_accepted_count = 0u;
    dev->rx_ignored_count = 0u;
    dev->rx_malformed_count = 0u;
    dev->rx_remote_count = 0u;
    dev->filters_configured = false;
    dev->started = false;
    dev->tx_queue = xQueueCreate(CANBUS_TX_QUEUE_LENGTH, sizeof(canbus_packet_t));

    dev->tx_header->IDE = CAN_ID_STD;
    dev->tx_header->StdId = 0x00;
    dev->tx_header->ExtId = 0x00;
    dev->tx_header->RTR = CAN_RTR_DATA;
    dev->tx_header->DLC = 8;
    dev->tx_header->TransmitGlobalTime = DISABLE;

    dev->filters_configured = (canbus_configure_rx_filters(hcan) == HAL_OK);
    dev->started = ((dev->tx_queue != NULL) &&
                    dev->filters_configured &&
                    (HAL_CAN_Start(hcan) == HAL_OK));
}

HAL_StatusTypeDef canbus_queue_tx(canbus_t *dev, const canbus_packet_t *packet)
{
    if((dev == NULL) || (packet == NULL) || (dev->tx_queue == NULL))
    {
        return HAL_ERROR;
    }

    if(uxQueueMessagesWaiting(dev->tx_queue) != 0u)
    {
        dev->tx_replaced_count++;
    }

    /* This is a latest-value mailbox, not a FIFO.  A newer disable request
     * must replace an older positive-torque command that has not been sent. */
    if(xQueueOverwrite(dev->tx_queue, packet) != pdPASS)
    {
        dev->tx_dropped_count++;
        return HAL_BUSY;
    }

    return HAL_OK;
}

HAL_StatusTypeDef canbus_transmit(canbus_t *dev, const canbus_packet_t *packet, uint32_t timeout_ms)
{
    uint32_t start;

    if((dev == NULL) || (packet == NULL) || (dev->hcan == NULL) || (dev->tx_header == NULL))
    {
        return HAL_ERROR;
    }

    start = HAL_GetTick();
    while(HAL_CAN_GetTxMailboxesFreeLevel(dev->hcan) == 0u)
    {
        if((uint32_t)(HAL_GetTick() - start) >= timeout_ms)
        {
            return HAL_TIMEOUT;
        }
        taskYIELD();
    }

    dev->tx_header->StdId = packet->id;
    dev->tx_header->DLC = DATALEN;
    return HAL_CAN_AddTxMessage(dev->hcan, dev->tx_header, (uint8_t *)packet->data, &dev->tx_mailbox);
}
