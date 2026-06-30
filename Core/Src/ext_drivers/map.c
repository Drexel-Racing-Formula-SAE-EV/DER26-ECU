/**
* @file map.c 
* @author Cole Bardin (cab572@drexel.edu)
* @brief
* @version 0.1
* @date 2023-03-13
*
* @copyright Copyright (c) 2023
*
*/

#include "ext_drivers/map.h"

// Parsed from Arduino's Wiring.h
float map(long x, long in_min, long in_max, long out_min, long out_max)
{
	long in_range = in_max - in_min;
	long out_range = out_max - out_min;
	float num;
	float result;

	if(in_range == 0L)
	{
		return (float)out_min + ((float)out_range / 2.0f);
	}

	num = (float)(x - in_min) * (float)out_range;
	if(out_range >= 0L)
	{
		num += (float)in_range / 2.0f;
	}
	else
	{
		num -= (float)in_range / 2.0f;
	}

	result = (num / (float)in_range) + (float)out_min;
	if(out_range >= 0L)
	{
		if(((float)in_range * num) < 0.0f)
		{
			return result - 1.0f;
		}
	}
	else
	{
		if(((float)in_range * num) >= 0.0f)
		{
			return result + 1.0f;
		}
	}
	return result;
}
