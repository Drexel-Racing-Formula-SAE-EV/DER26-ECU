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

#ifndef ECU_EXT_DRIVERS_CANBUS_H_
#define ECU_EXT_DRIVERS_CANBUS_H_

#include "cmsis_os.h"
#include "stm32f7xx_hal.h"

#define DATALEN 8u
#define CANBUS_TX_TIMEOUT_MS 10u

typedef struct {
    uint32_t id;
    uint8_t data[DATALEN];
} canbus_packet_t;

typedef struct {
    CAN_HandleTypeDef *hcan;
    CAN_TxHeaderTypeDef *tx_header;
    uint32_t tx_mailbox;
    canbus_packet_t rx_packet;
    canbus_packet_t tx_packet;
} canbus_t;

void canbus_device_init(canbus_t *dev, CAN_HandleTypeDef *hcan, CAN_TxHeaderTypeDef *tx_header);
HAL_StatusTypeDef canbus_transmit(canbus_t *dev, const canbus_packet_t *packet, uint32_t timeout_ms);

#endif
