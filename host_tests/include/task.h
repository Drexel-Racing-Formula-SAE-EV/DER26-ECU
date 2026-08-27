#ifndef HOST_TASK_H
#define HOST_TASK_H
#include <stdint.h>
#include "FreeRTOS.h"
typedef void *TaskHandle_t;
void taskYIELD(void);
TaskHandle_t xTaskCreateStatic(void (*task_fn)(void *), const char *name,
                              uint32_t stack_depth, void *arg,
                              UBaseType_t priority, StackType_t *stack,
                              StaticTask_t *tcb);
#define taskENTER_CRITICAL() do { } while(0)
#define taskEXIT_CRITICAL() do { } while(0)
#endif
