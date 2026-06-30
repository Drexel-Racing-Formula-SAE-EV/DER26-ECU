/*
 * flow_sensor.c
 *
 *  Created on: Mar 26, 2024
 *      Author: cole
 */

#include "ext_drivers/flow_sensor.h"

void flow_sensor_init(flow_sensor_t *dev, uint32_t clock_freq, TIM_HandleTypeDef *htim, TIM_TypeDef *tim, HAL_TIM_ActiveChannel high_channel, HAL_TIM_ActiveChannel total_channel)
{
	if((dev == NULL) || (htim == NULL))
	{
		return;
	}

	dev->clock_freq = clock_freq;
	dev->htim = htim;
	dev->tim = tim;
	dev->high_channel = high_channel;
	dev->total_channel = total_channel;
	dev->duty = 0.0f;
	dev->freq = 0.0f;
	dev->high_count = 0u;
	dev->total_count = 0u;
	dev->ret = 0;

	if(HAL_TIM_Base_Start(htim) != HAL_OK)
	{
		dev->ret = -1;
	}
	if(HAL_TIM_IC_Start_IT(htim, total_channel) != HAL_OK)
	{
		dev->ret = -1;
	}
	if(HAL_TIM_IC_Start(htim, high_channel) != HAL_OK)
	{
		dev->ret = -1;
	}
}

int flow_sensor_read(flow_sensor_t *dev)
{
	if((dev == NULL) || (dev->htim == NULL) || (dev->clock_freq == 0u))
	{
		return -1;
	}

	dev->total_count = HAL_TIM_ReadCapturedValue(dev->htim, dev->total_channel);
	if (dev->total_count != 0u)
	{
		dev->high_count = HAL_TIM_ReadCapturedValue(dev->htim, dev->high_channel);
		if(dev->high_count > dev->total_count)
		{
			dev->high_count = dev->total_count;
		}
		dev->duty = ((float)dev->high_count * 100.0f) / (float)dev->total_count;
		dev->freq = (float)dev->clock_freq / (float)dev->total_count;
	}
	else
	{
		dev->high_count = 0u;
		dev->duty = 0.0f;
		dev->freq = 0.0f;
	}
	return 0;
}
