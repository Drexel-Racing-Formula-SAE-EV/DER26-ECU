/**
* @file stm32f767.c
* @author Cole Bardin (cab572@drexel.edu)
* @brief
* @version 0.1
* @date 2023-03-13
*
* @copyright Copyright (c) 2023
*
*/

#include "main.h"
#include "ext_drivers/stm32f767.h"
#include "ext_drivers/canbus.h"
#include "semphr.h"

static StaticSemaphore_t can1_mutex_cb;
static StaticSemaphore_t adc3_mutex_cb;
static StaticSemaphore_t i2c2_mutex_cb;
static StaticSemaphore_t spi6_mutex_cb;
static StaticSemaphore_t uart3_mutex_cb;
static StaticSemaphore_t uart7_mutex_cb;
static bool dwt_cycle_counter_available = false;

const osMutexAttr_t can1_mutex_attr = {
	.name = "CAN Bus Mutex",
	.attr_bits = osMutexPrioInherit | osMutexRecursive,
	.cb_mem = &can1_mutex_cb,
	.cb_size = sizeof(can1_mutex_cb),
};

const osMutexAttr_t adc3_mutex_attr = {
	.name = "ADC3 Mutex",
	.attr_bits = osMutexPrioInherit | osMutexRecursive,
	.cb_mem = &adc3_mutex_cb,
	.cb_size = sizeof(adc3_mutex_cb),
};

const osMutexAttr_t i2c2_mutex_attr = {
	.name = "MPU6050 Mutex",
	.attr_bits = osMutexPrioInherit | osMutexRecursive,
	.cb_mem = &i2c2_mutex_cb,
	.cb_size = sizeof(i2c2_mutex_cb),
};

const osMutexAttr_t spi6_mutex_attr = {
	.name = "SD Card Mutex",
	.attr_bits = osMutexPrioInherit | osMutexRecursive,
	.cb_mem = &spi6_mutex_cb,
	.cb_size = sizeof(spi6_mutex_cb),
};

const osMutexAttr_t uart3_mutex_attr = {
	.name = "CLI Mutex",
	.attr_bits = osMutexPrioInherit | osMutexRecursive,
	.cb_mem = &uart3_mutex_cb,
	.cb_size = sizeof(uart3_mutex_cb),
};

const osMutexAttr_t uart7_mutex_attr = {
	.name = "Dashboard Mutex",
	.attr_bits = osMutexPrioInherit | osMutexRecursive,
	.cb_mem = &uart7_mutex_cb,
	.cb_size = sizeof(uart7_mutex_cb),
};

bool stm32f767_cycle_counter_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    dwt_cycle_counter_available = ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0u);
    return dwt_cycle_counter_available;
}

bool stm32f767_cycle_counter_available(void)
{
    return dwt_cycle_counter_available;
}

uint32_t stm32f767_cycle_counter_read(void)
{
    return dwt_cycle_counter_available ? DWT->CYCCNT : 0u;
}

uint32_t stm32f767_cycles_to_us(uint32_t cycles)
{
    if((!dwt_cycle_counter_available) || (SystemCoreClock == 0u))
    {
        return 0u;
    }

    return (uint32_t)(((uint64_t)cycles * 1000000ull) /
                      (uint64_t)SystemCoreClock);
}

void stm32f767_init(stm32f767_t *dev)
{
	extern ADC_HandleTypeDef hadc1;
	extern ADC_HandleTypeDef hadc2;
	extern ADC_HandleTypeDef hadc3;

	extern CAN_HandleTypeDef hcan1;

	extern I2C_HandleTypeDef hi2c2;

	extern RTC_HandleTypeDef hrtc;

	extern SPI_HandleTypeDef hspi6;

	extern TIM_HandleTypeDef htim3;
	extern TIM_HandleTypeDef htim4;
	extern TIM_HandleTypeDef htim5;

	extern UART_HandleTypeDef huart7;
	extern UART_HandleTypeDef huart3;

	if(dev == NULL)
	{
		return;
	}

	dev->hadc1 = &hadc1;
	dev->hadc2 = &hadc2;
	dev->hadc3 = &hadc3;
	dev->hcan1 = &hcan1;
	dev->hi2c2 = &hi2c2;
	dev->hrtc = &hrtc;
	dev->hspi6 = &hspi6;
	dev->htim3 = &htim3;
	dev->htim4 = &htim4;
	dev->htim5 = &htim5;
	dev->huart3 = &huart3;
	dev->huart7 = &huart7;

	dev->can1_mutex = osMutexNew(&can1_mutex_attr);

	dev->adc3_mutex = osMutexNew(&adc3_mutex_attr);

	dev->i2c2_mutex = osMutexNew(&i2c2_mutex_attr);

	dev->spi6_mutex = osMutexNew(&spi6_mutex_attr);

	dev->uart3_mutex = osMutexNew(&uart3_mutex_attr);

	dev->uart7_mutex = osMutexNew(&uart7_mutex_attr);

	(void)stm32f767_cycle_counter_init();

	dev->initialized = ((dev->can1_mutex != NULL) &&
	                    (dev->adc3_mutex != NULL) &&
	                    (dev->i2c2_mutex != NULL) &&
	                    (dev->spi6_mutex != NULL) &&
	                    (dev->uart3_mutex != NULL) &&
	                    (dev->uart7_mutex != NULL));
}

uint16_t stm32f767_adc_read(ADC_HandleTypeDef *hadc)
{
	uint16_t count = 0u;
	(void)stm32f767_adc_read_checked(hadc, &count);
	return count;
}

HAL_StatusTypeDef stm32f767_adc_read_checked(ADC_HandleTypeDef *hadc, uint16_t *count)
{
	HAL_StatusTypeDef status;

	if((hadc == NULL) || (count == NULL))
	{
		return HAL_ERROR;
	}

	status = HAL_ADC_Start(hadc);
	if(status != HAL_OK)
	{
		return status;
	}

	status = HAL_ADC_PollForConversion(hadc, 10u);
	if(status == HAL_OK)
	{
		*count = (uint16_t)HAL_ADC_GetValue(hadc);
	}

	if((HAL_ADC_Stop(hadc) != HAL_OK) && (status == HAL_OK))
	{
		status = HAL_ERROR;
	}
	return status;
}

HAL_StatusTypeDef stm32f767_adc_switch_channel(ADC_HandleTypeDef *hadc, uint32_t channel)
{
	if(hadc == NULL)
	{
		return HAL_ERROR;
	}

	ADC_ChannelConfTypeDef sConfig = {0};
	sConfig.Channel = channel;
	sConfig.Rank = ADC_REGULAR_RANK_1;
	sConfig.SamplingTime = ADC_SAMPLETIME_56CYCLES;
	return HAL_ADC_ConfigChannel(hadc, &sConfig);
}
