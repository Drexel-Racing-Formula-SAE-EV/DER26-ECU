/*
 * pwm.c
 *
 *  Created on: Mar 28, 2024
 *      Author: cole
 */

#include "ext_drivers/pwm.h"
#include <math.h>
#include <stdint.h>

int pwm_device_init(pwm_t *dev, TIM_TypeDef *timer, TIM_HandleTypeDef *htim, uint64_t max_timer_val, volatile uint32_t *CCR, int channel)
{
	if((dev == NULL) || (htim == NULL) || (CCR == NULL) ||
	   (max_timer_val == 0u) || (max_timer_val > UINT32_MAX) ||
	   (channel < 1) || (channel > 4))
	{
		return -1;
	}

	dev->timer = timer;
	dev->htim = htim;
	dev->channel = channel;
	dev->max_timer_val = max_timer_val;
	dev->CCR = CCR;

	if(HAL_TIM_PWM_Start(htim, (uint32_t)(channel - 1) * 4u) != HAL_OK)
	{
		return -1;
	}
	return pwm_set_percent(dev, 0.0f);
}

int pwm_set_percent(pwm_t *dev, float percent)
{
	if((dev == NULL) || (dev->CCR == NULL) ||
	   (dev->max_timer_val == 0u) || !isfinite(percent))
	{
		return -1;
	}

	if(percent > 100.0f) percent = 100.0f;
	else if(percent < 0.0f) percent = 0.0f;
	dev->duty_cycle = percent;
	*(dev->CCR) = (uint32_t)(((float)dev->max_timer_val * percent) / 100.0f);
	return 0;
}
