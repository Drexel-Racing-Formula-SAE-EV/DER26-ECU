#ifndef HOST_TEST_CMSIS_OS_H_
#define HOST_TEST_CMSIS_OS_H_

#include <stdint.h>
#include <stdbool.h>

typedef void *osMutexId_t;
typedef void *TaskHandle_t;
typedef uint32_t osStatus_t;

typedef struct
{
    const char *name;
    uint32_t attr_bits;
    void *cb_mem;
    uint32_t cb_size;
} osMutexAttr_t;

#define osMutexPrioInherit 0x01u
#define osMutexRecursive 0x02u

void taskYIELD(void);
int xPortIsInsideInterrupt(void);
osMutexId_t osMutexNew(const osMutexAttr_t *attr);

#endif /* HOST_TEST_CMSIS_OS_H_ */
