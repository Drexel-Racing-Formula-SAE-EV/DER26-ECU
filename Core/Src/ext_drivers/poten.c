/**
 * @file poten.c
 * @author Cole Bardin (cab572@drexel.edu)
 * @brief 
 * @version 0.1
 * @date 2023-03-14
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <math.h>

#include "ext_drivers/poten.h"
#include "ext_drivers/map.h"

void poten_init(poten_t *poten, uint16_t min, uint16_t max, ADC_HandleTypeDef *handle)
{
	if(poten == NULL)
	{
		return;
	}

	poten->min = min;
	poten->max = max;
	poten->handle = handle;
	poten->count = 0u;
	poten->percent = 0.0f;
	for(uint16_t i = 0u; i < HISTSZ; i++)
	{
		poten->hist[i] = 0.0f;
	}
}

float poten_get_percent(poten_t *root)
{
	float percent = 0.0f;
	float raw;

	if(root == NULL)
	{
		return 0.0f;
	}

	raw = map((long)root->count, (long)root->min, (long)root->max, 0L, 100L);
	if(raw > 100.0f)
	{
		raw = 100.0f;
	}
	else if(raw < 0.0f)
	{
		raw = 0.0f;
	}

	for(uint16_t i = 0u; i < (uint16_t)(HISTSZ - 1u); i++)
	{
		uint16_t dst = (uint16_t)(HISTSZ - i - 1u);
		uint16_t src = (uint16_t)(HISTSZ - i - 2u);
		root->hist[dst] = root->hist[src];
		percent += root->hist[dst];
	}
	root->hist[0] = raw;
	percent += raw;
	percent /= (float)HISTSZ;
	root->percent = percent;
	return percent;
}

uint16_t poten_percent_to_hex(float percent)
{
	if(percent > 100.0f)
	{
		percent = 100.0f;
	}
	else if(percent < 0.0f)
	{
		percent = 0.0f;
	}

	return (uint16_t)((percent * 65535.0f) / 100.0f);
}

uint8_t poten_check_plausibility(float L, float R, int thresh, int count)
{
    static unsigned int counts = 0;

	// Check if APPS1 and APPS2 are more than 10% different
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

uint8_t poten_check_failure(float count, int max_thresh, int min_thresh)
{
	if((count > (float)max_thresh) || (count < (float)min_thresh))
	{
		return 0u;
	}
	return 1u;
}
