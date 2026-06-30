#ifndef HOST_TEST_STM32F7XX_HAL_RTC_H_
#define HOST_TEST_STM32F7XX_HAL_RTC_H_

#include "stm32f7xx_hal.h"

typedef struct
{
    uint8_t Hours;
    uint8_t Minutes;
    uint8_t Seconds;
} RTC_TimeTypeDef;

typedef struct
{
    uint8_t WeekDay;
    uint8_t Month;
    uint8_t Date;
    uint8_t Year;
} RTC_DateTypeDef;

#endif /* HOST_TEST_STM32F7XX_HAL_RTC_H_ */
