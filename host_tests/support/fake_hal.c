#include "support/fake_hal.h"
#include "cmsis_os.h"
#include "task.h"

#include <string.h>

host_fake_hal_t host_hal;
TIM_TypeDef host_tim3;
TIM_TypeDef host_tim4;
TIM_TypeDef host_tim5;

void host_hal_reset(void)
{
    memset(&host_hal, 0, sizeof(host_hal));
    memset(&host_tim3, 0, sizeof(host_tim3));
    memset(&host_tim4, 0, sizeof(host_tim4));
    memset(&host_tim5, 0, sizeof(host_tim5));
    host_hal.tick_step = 1u;
    host_hal.tim_pwm_status = HAL_OK;
    host_hal.tim_base_status = HAL_OK;
    host_hal.tim_ic_it_status = HAL_OK;
    host_hal.tim_ic_status = HAL_OK;
    host_hal.uart_status = HAL_OK;
    host_hal.i2c_ready_status = HAL_OK;
    host_hal.i2c_write_status = HAL_OK;
    host_hal.i2c_read_status = HAL_OK;
    host_hal.can_filter_status = HAL_OK;
    host_hal.can_filter_fail_index = -1;
    host_hal.can_start_status = HAL_OK;
    host_hal.can_add_status = HAL_OK;
    host_hal.can_abort_status = HAL_OK;
    host_hal.can_free_level = 1u;
}

bool xPortIsInsideInterrupt(void)
{
    return host_hal.inside_isr;
}

HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *htim, uint32_t channel)
{
    (void)htim;
    host_hal.tim_pwm_calls++;
    host_hal.tim_pwm_channel = channel;
    return host_hal.tim_pwm_status;
}

HAL_StatusTypeDef HAL_TIM_Base_Start(TIM_HandleTypeDef *htim)
{
    (void)htim;
    host_hal.tim_base_calls++;
    return host_hal.tim_base_status;
}

HAL_StatusTypeDef HAL_TIM_IC_Start_IT(TIM_HandleTypeDef *htim, uint32_t channel)
{
    (void)htim;
    (void)channel;
    host_hal.tim_ic_it_calls++;
    return host_hal.tim_ic_it_status;
}

HAL_StatusTypeDef HAL_TIM_IC_Start(TIM_HandleTypeDef *htim, uint32_t channel)
{
    (void)htim;
    (void)channel;
    host_hal.tim_ic_calls++;
    return host_hal.tim_ic_status;
}

static size_t channel_index(uint32_t channel)
{
    switch(channel)
    {
    case TIM_CHANNEL_1: return 0u;
    case TIM_CHANNEL_2: return 1u;
    case TIM_CHANNEL_3: return 2u;
    case TIM_CHANNEL_4: return 3u;
    default: return 0u;
    }
}

uint32_t HAL_TIM_ReadCapturedValue(TIM_HandleTypeDef *htim, uint32_t channel)
{
    (void)htim;
    return host_hal.capture[channel_index(channel)];
}

uint32_t HAL_GetTick(void)
{
    uint32_t now = host_hal.tick;
    host_hal.tick += host_hal.tick_step;
    return now;
}

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, uint8_t *data,
                                    uint16_t len, uint32_t timeout)
{
    (void)huart;
    (void)timeout;
    host_hal.uart_calls++;
    size_t space = HOST_UART_CAPTURE_SIZE - host_hal.uart_capture_len;
    size_t copy = (len < space) ? len : space;
    if(copy > 0u)
    {
        memcpy(&host_hal.uart_capture[host_hal.uart_capture_len], data, copy);
        host_hal.uart_capture_len += copy;
    }
    return host_hal.uart_status;
}

HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *hi2c,
                                        uint16_t dev_addr, uint32_t trials,
                                        uint32_t timeout)
{
    (void)hi2c; (void)dev_addr; (void)trials; (void)timeout;
    return host_hal.i2c_ready_status;
}

HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c,
                                    uint16_t dev_addr, uint16_t mem_addr,
                                    uint16_t mem_addr_size, uint8_t *data,
                                    uint16_t len, uint32_t timeout)
{
    (void)hi2c; (void)mem_addr_size; (void)timeout;
    if(host_hal.i2c_write_count < HOST_MAX_I2C_WRITES && len > 0u)
    {
        host_i2c_write_t *w = &host_hal.i2c_writes[host_hal.i2c_write_count++];
        w->dev_addr = dev_addr;
        w->mem_addr = mem_addr;
        w->value = data[0];
    }
    return host_hal.i2c_write_status;
}

HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *hi2c,
                                   uint16_t dev_addr, uint16_t mem_addr,
                                   uint16_t mem_addr_size, uint8_t *data,
                                   uint16_t len, uint32_t timeout)
{
    (void)hi2c; (void)dev_addr; (void)mem_addr; (void)mem_addr_size; (void)timeout;
    size_t copy = len;
    if(copy > host_hal.i2c_read_len) copy = host_hal.i2c_read_len;
    if(copy > 0u) memcpy(data, host_hal.i2c_read_data, copy);
    if(copy < len) memset(data + copy, 0, len - copy);
    return host_hal.i2c_read_status;
}

HAL_StatusTypeDef HAL_CAN_ConfigFilter(CAN_HandleTypeDef *hcan,
                                       CAN_FilterTypeDef *filter)
{
    (void)hcan;
    size_t index = host_hal.can_filter_count;
    if(index < HOST_MAX_CAN_FILTERS)
    {
        host_hal.can_filters[index] = *filter;
    }
    host_hal.can_filter_count++;
    if(host_hal.can_filter_fail_index >= 0 &&
       (int32_t)index == host_hal.can_filter_fail_index)
    {
        return HAL_ERROR;
    }
    return host_hal.can_filter_status;
}

HAL_StatusTypeDef HAL_CAN_Start(CAN_HandleTypeDef *hcan)
{
    (void)hcan;
    host_hal.can_start_calls++;
    return host_hal.can_start_status;
}

uint32_t HAL_CAN_GetTxMailboxesFreeLevel(CAN_HandleTypeDef *hcan)
{
    (void)hcan;
    return host_hal.can_free_level;
}

HAL_StatusTypeDef HAL_CAN_AddTxMessage(CAN_HandleTypeDef *hcan,
                                       CAN_TxHeaderTypeDef *header,
                                       uint8_t data[], uint32_t *mailbox)
{
    (void)hcan;
    host_hal.can_add_calls++;
    host_hal.can_last_header = *header;
    memcpy(host_hal.can_last_payload, data, sizeof(host_hal.can_last_payload));
    if(mailbox != NULL) *mailbox = CAN_TX_MAILBOX0;
    return host_hal.can_add_status;
}

HAL_StatusTypeDef HAL_CAN_AbortTxRequest(CAN_HandleTypeDef *hcan,
                                        uint32_t mailbox)
{
    (void)hcan;
    host_hal.can_abort_calls++;
    host_hal.can_last_abort_mailbox = mailbox;
    return host_hal.can_abort_status;
}

void osDelay(uint32_t ms)
{
    host_hal.tick += ms;
    if(host_hal.delay_hook != NULL)
    {
        host_hal.delay_hook();
    }
}

QueueHandle_t xQueueCreateStatic(UBaseType_t length, UBaseType_t item_size,
                                 uint8_t *storage, StaticQueue_t *control)
{
    if(length == 0u || item_size == 0u || storage == NULL || control == NULL)
    {
        return NULL;
    }
    control->storage = storage;
    control->item_size = item_size;
    control->length = length;
    control->messages = 0u;
    control->force_fail = pdFAIL;
    return control;
}

BaseType_t xQueueOverwrite(QueueHandle_t queue, const void *item)
{
    if(queue == NULL || item == NULL || queue->force_fail == pdPASS)
    {
        return pdFAIL;
    }
    memcpy(queue->storage, item, queue->item_size);
    queue->messages = 1u;
    return pdPASS;
}

UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue)
{
    return (queue == NULL) ? 0u : queue->messages;
}

void host_queue_force_fail(QueueHandle_t queue, bool fail)
{
    if(queue != NULL) queue->force_fail = fail ? pdPASS : pdFAIL;
}

void taskYIELD(void)
{
    host_hal.yield_calls++;
}
