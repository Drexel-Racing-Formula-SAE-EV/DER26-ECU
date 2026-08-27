#ifndef ECU_HOST_FAKE_HAL_H
#define ECU_HOST_FAKE_HAL_H

#include "stm32f7xx_hal.h"
#include "queue.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HOST_MAX_CAN_FILTERS 16u
#define HOST_MAX_I2C_WRITES 16u
#define HOST_UART_CAPTURE_SIZE 1024u

typedef struct {
    uint16_t dev_addr;
    uint16_t mem_addr;
    uint8_t value;
} host_i2c_write_t;

typedef struct {
    uint32_t tick;
    uint32_t tick_step;
    bool inside_isr;

    HAL_StatusTypeDef tim_pwm_status;
    HAL_StatusTypeDef tim_base_status;
    HAL_StatusTypeDef tim_ic_it_status;
    HAL_StatusTypeDef tim_ic_status;
    uint32_t tim_pwm_calls;
    uint32_t tim_pwm_channel;
    uint32_t tim_base_calls;
    uint32_t tim_ic_it_calls;
    uint32_t tim_ic_calls;
    uint32_t capture[4];

    HAL_StatusTypeDef uart_status;
    uint8_t uart_capture[HOST_UART_CAPTURE_SIZE];
    size_t uart_capture_len;
    uint32_t uart_calls;

    HAL_StatusTypeDef i2c_ready_status;
    HAL_StatusTypeDef i2c_write_status;
    HAL_StatusTypeDef i2c_read_status;
    host_i2c_write_t i2c_writes[HOST_MAX_I2C_WRITES];
    size_t i2c_write_count;
    uint8_t i2c_read_data[32];
    size_t i2c_read_len;

    HAL_StatusTypeDef can_filter_status;
    int32_t can_filter_fail_index;
    HAL_StatusTypeDef can_start_status;
    HAL_StatusTypeDef can_add_status;
    HAL_StatusTypeDef can_abort_status;
    void (*delay_hook)(void);
    CAN_FilterTypeDef can_filters[HOST_MAX_CAN_FILTERS];
    size_t can_filter_count;
    uint32_t can_free_level;
    uint32_t can_start_calls;
    uint32_t can_add_calls;
    uint32_t can_abort_calls;
    uint32_t can_last_abort_mailbox;
    CAN_TxHeaderTypeDef can_last_header;
    uint8_t can_last_payload[8];
    uint32_t yield_calls;
} host_fake_hal_t;

extern host_fake_hal_t host_hal;
extern TIM_TypeDef host_tim3;
extern TIM_TypeDef host_tim4;
extern TIM_TypeDef host_tim5;

void host_hal_reset(void);
void host_queue_force_fail(QueueHandle_t queue, bool fail);

#endif
