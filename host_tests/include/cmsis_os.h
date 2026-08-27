#ifndef HOST_CMSIS_OS_H
#define HOST_CMSIS_OS_H
#include <stdbool.h>
#include <stdint.h>
typedef void *osMutexId_t;
typedef void *TaskHandle_t;
bool xPortIsInsideInterrupt(void);
void taskYIELD(void);
void osDelay(uint32_t ms);
#endif
