/**
* @file map.c
* @author Cole Bardin (cab572@drexel.edu)
* @brief Linear range conversion used by analog sensor drivers.
* @version 0.2
* @date 2026-08-06
*/

#include "ext_drivers/map.h"

long double map(long x, long in_min, long in_max, long out_min, long out_max)
{
    const long in_range = in_max - in_min;
    const long out_range = out_max - out_min;

    if(in_range == 0)
    {
        return (long double)out_min + ((long double)out_range / 2.0L);
    }

    return (long double)out_min +
           (((long double)(x - in_min) * (long double)out_range) /
            (long double)in_range);
}
