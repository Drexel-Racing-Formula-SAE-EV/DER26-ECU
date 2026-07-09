/**
 * @file canbus.h
 * @author Cole Bardin (cab572@drexel.edu)
 * @brief 
 * @version 0.1
 * @date 2023-04-24
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef __CANBUS_H_
#define __CANBUS_H_

#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "stm32f7xx_hal.h"

#define DATALEN 8
#define CANBUS_TX_TIMEOUT_MS 1u
#define CANBUS_TX_QUEUE_LENGTH 8u

typedef struct {
    uint32_t id;
    uint8_t data[DATALEN];
} canbus_packet_t;

typedef struct {
    CAN_HandleTypeDef *hcan;
    CAN_TxHeaderTypeDef *tx_header;
    uint32_t tx_mailbox;
    canbus_packet_t rx_packet;
    canbus_packet_t tx_packet; /* Legacy single-slot path; prefer tx_queue for new code. */
    QueueHandle_t tx_queue;
    uint32_t tx_dropped_count;
} canbus_t;

void canbus_device_init(canbus_t *dev, CAN_HandleTypeDef *hcan, CAN_TxHeaderTypeDef *tx_header);
HAL_StatusTypeDef canbus_transmit(canbus_t *dev, const canbus_packet_t *packet, uint32_t timeout_ms);
HAL_StatusTypeDef canbus_queue_tx(canbus_t *dev, const canbus_packet_t *packet);

#endif
