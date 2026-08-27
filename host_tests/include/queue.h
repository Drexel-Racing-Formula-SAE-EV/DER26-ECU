#ifndef HOST_QUEUE_H
#define HOST_QUEUE_H
#include "FreeRTOS.h"
#include <stddef.h>
#include <stdint.h>
typedef struct HostStaticQueue {
    uint8_t *storage;
    size_t item_size;
    UBaseType_t length;
    UBaseType_t messages;
    BaseType_t force_fail;
} StaticQueue_t;
typedef StaticQueue_t *QueueHandle_t;
QueueHandle_t xQueueCreateStatic(UBaseType_t length, UBaseType_t item_size,
                                 uint8_t *storage, StaticQueue_t *control);
BaseType_t xQueueOverwrite(QueueHandle_t queue, const void *item);
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue);
#endif
