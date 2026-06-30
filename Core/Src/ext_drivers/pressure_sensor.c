/**
* @file pressure_sensor.c
* @author Cole Bardin (cab572@drexel.edu)
* @brief
* @version 0.1
* @date 2023-04-24
*
* @copyright Copyright (c) 2023
*
*/

#include <math.h>

#include "ext_drivers/pressure_sensor.h"

#include "ext_drivers/map.h"

void pressure_sensor_init(pressure_sensor_t *sensor, uint16_t min, uint16_t max, ADC_HandleTypeDef *handle, uint8_t channel)
{
	if(sensor == NULL)
	{
		return;
	}

	sensor->min = min;
	sensor->max = max;
	sensor->count = 0u;
	sensor->percent = 0.0f;
	sensor->handle = handle;
	sensor->channel = channel;
}

float pressure_sensor_get_percent(pressure_sensor_t *root)
{
	float percent;

	if(root == NULL)
	{
		return 0.0f;
	}

	percent = map((long)root->count, (long)root->min, (long)root->max, 0L, 100L);
	if(percent > 100.0f)
	{
		percent = 100.0f;
	}
	else if(percent < 0.0f)
	{
		percent = 0.0f;
	}

	root->percent = percent;
	return percent;
}

uint8_t pressure_sensor_check_implausibility(float L, float R, int thresh, int count)
{
    static unsigned int counts = 0;

	// Check if BSE1 and BSE2 are more than 10% different
	if(fabsf(L - R) > (float)thresh)
	{
		counts++;
		// If there are consecutive errors for more than 100ms, error
		return (counts <= (uint32_t)count) ? 1u : 0u;
	}
	else
	{
		// If potentiometers are within spec, reset count
		counts = 0;
		return 1u;
	}
}

uint8_t pressure_sensor_check_failure(float count, int max_thresh, int min_thresh)
{
	if((count > (float)max_thresh) || (count < (float)min_thresh))
	{
		return 0u;
	}
	return 1u;
}
