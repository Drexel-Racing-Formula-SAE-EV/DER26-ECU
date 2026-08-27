/*
 * dashboard.c
 *
 *  Created on: Mar 23, 2024
 *      Author: cole
 */

#include <string.h>

#include "ext_drivers/dashboard.h"

int dashboard_init(dashboard_t *dev, UART_HandleTypeDef *huart)
{
	if((dev == NULL) || (huart == NULL)) return -1;
	dev->huart = huart;
	dev->ret = HAL_OK;
	memset(dev->line, 0, DASH_LINESZ);
	return 0;
}

HAL_StatusTypeDef dashboard_write(dashboard_t *dev, char *str)
{
	HAL_StatusTypeDef ret;
	if((dev == NULL) || (dev->huart == NULL) || (str == NULL)) return HAL_ERROR;
	ret = HAL_UART_Transmit(dev->huart, (uint8_t *)str, strlen(str), 200);
	return ret;
}
