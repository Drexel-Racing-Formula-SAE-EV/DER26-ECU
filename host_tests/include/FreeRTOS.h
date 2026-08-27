#ifndef HOST_FREERTOS_H
#define HOST_FREERTOS_H
#include <stddef.h>
#include <stdint.h>
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef uint32_t StackType_t;
typedef struct { uintptr_t opaque[8]; } StaticTask_t;
#define pdPASS 1
#define pdFAIL 0
#endif
