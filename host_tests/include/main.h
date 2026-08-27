#ifndef HOST_MAIN_H
#define HOST_MAIN_H
#include "stm32f7xx_hal.h"
extern TIM_TypeDef host_tim3;
extern TIM_TypeDef host_tim4;
extern TIM_TypeDef host_tim5;
#define TIM3 (&host_tim3)
#define TIM4 (&host_tim4)
#define TIM5 (&host_tim5)
#endif
