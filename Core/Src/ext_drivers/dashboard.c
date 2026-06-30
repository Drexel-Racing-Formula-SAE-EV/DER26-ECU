/*
 * dashboard.c
 *
 *  Created on: Mar 23, 2024
 *      Author: cole
 */

#include <string.h>

#include "ext_drivers/dashboard.h"

static uint16_t dashboard_strlen_u16(const char *str)
{
	uint16_t len = 0u;

	if(str == NULL)
	{
		return 0u;
	}

	while((str[len] != '\0') && (len < UINT16_MAX))
	{
		len++;
	}

	return len;
}

int dashboard_init(dashboard_t *dev, UART_HandleTypeDef *huart)
{
	if(dev == NULL)
	{
		return (int)HAL_ERROR;
	}

	dev->huart = huart;
	dev->ret = HAL_OK;
	memset(dev->line, 0, sizeof(dev->line));
	return (int)HAL_OK;
}

HAL_StatusTypeDef dashboard_write(dashboard_t *dev, const char *str)
{
	if((dev == NULL) || (dev->huart == NULL) || (str == NULL))
	{
		return HAL_ERROR;
	}

	return HAL_UART_Transmit(dev->huart, (uint8_t *)str, dashboard_strlen_u16(str), 200u);
}
