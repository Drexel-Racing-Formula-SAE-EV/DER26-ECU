/**
 * @file rtc.h
 * @author Cole Bardin (cab572@drexel.edu)
 * @brief 
 * @version 0.1
 * @date 2024-01-10
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef ECU_EXT_DRIVERS_RTC_H_
#define ECU_EXT_DRIVERS_RTC_H_

#include <stdint.h>
#include <stm32f7xx_hal.h>
#include <stm32f7xx_hal_rtc.h>

static inline uint16_t rtc_bcd_to_dec(uint16_t value)
{
	return (uint16_t)((10u * (value / 16u)) + (value % 16u));
}

static inline uint16_t rtc_dec_to_bcd(uint16_t value)
{
	return (uint16_t)((16u * (value / 10u)) + (value % 10u));
}

typedef struct {
	uint16_t year;
	uint16_t month;
	uint16_t day;
	uint16_t hour;
	uint16_t minute;
	uint16_t second;
} datetime_t;

#endif
