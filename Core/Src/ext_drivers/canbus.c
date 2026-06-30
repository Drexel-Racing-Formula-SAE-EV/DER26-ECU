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

void canbus_device_init(canbus_t *dev, CAN_HandleTypeDef *hcan, CAN_TxHeaderTypeDef *tx_header)
{
    if((dev == NULL) || (hcan == NULL) || (tx_header == NULL))
    {
        return;
    }

    dev->hcan = hcan;
    dev->tx_header = tx_header;

    dev->tx_header->IDE = CAN_ID_STD;
    dev->tx_header->StdId = 0x00;
    dev->tx_header->ExtId = 0x00;
    dev->tx_header->RTR = CAN_RTR_DATA;
    dev->tx_header->DLC = 8;
    dev->tx_header->TransmitGlobalTime = DISABLE;

    (void)HAL_CAN_Start(hcan);
}


HAL_StatusTypeDef canbus_transmit(canbus_t *dev, const canbus_packet_t *packet, uint32_t timeout_ms)
{
    uint32_t start;

    if((dev == NULL) || (packet == NULL) || (dev->hcan == NULL) || (dev->tx_header == NULL))
    {
        return HAL_ERROR;
    }

    if(packet->id > 0x7FFu)
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
