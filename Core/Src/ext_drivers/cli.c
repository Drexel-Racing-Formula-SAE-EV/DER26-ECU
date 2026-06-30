/**
 * @file cli.c
 * @author Cole Bardin (cab572@drexel.edu)
 * @brief 
 * @version 0.1
 * @date 2023-10-19
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include <string.h>

#include "cmsis_os.h"

#include "ext_drivers/cli.h"

static HAL_StatusTypeDef merge_hal_status(HAL_StatusTypeDef current, HAL_StatusTypeDef next)
{
	HAL_StatusTypeDef result = current;

	if((result == HAL_OK) && (next != HAL_OK))
	{
		result = next;
	}

	return result;
}

static uint16_t cli_strlen_u16(const char *str)
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


void cli_device_init(cli_t *dev, UART_HandleTypeDef *huart)
{
	if(dev == NULL)
	{
		return;
	}

	dev->huart = huart;
	dev->index = 0u;
	dev->msg_pending = false;
	dev->msg_count = 0u;
	dev->msg_proc = 0u;
	dev->msg_valid = 0u;
	dev->ret = HAL_OK;
	memset(dev->line, 0, sizeof(dev->line));
}

int cli_printline(cli_t *dev, const char *line)
{
	static const char nl[] = "\r\n";
	HAL_StatusTypeDef ret = HAL_OK;

	if((dev == NULL) || (dev->huart == NULL) || (line == NULL))
	{
		return (int)HAL_ERROR;
	}

	if(xPortIsInsideInterrupt())
	{
		ret = merge_hal_status(ret, HAL_UART_Transmit_IT(dev->huart, (uint8_t *)line, cli_strlen_u16(line)));
		ret = merge_hal_status(ret, HAL_UART_Transmit_IT(dev->huart, (uint8_t *)nl, cli_strlen_u16(nl)));
	}
	else
	{
		ret = merge_hal_status(ret, HAL_UART_Transmit(dev->huart, (uint8_t *)line, cli_strlen_u16(line), 100u));
		ret = merge_hal_status(ret, HAL_UART_Transmit(dev->huart, (uint8_t *)nl, cli_strlen_u16(nl), 100u));
	}
	return (int)ret;
}

int tokenize(char *s, char *toks[], int maxtoks, const char *delim)
{
	int count = 0;
	char *cursor;

	if((s == NULL) || (toks == NULL) || (delim == NULL) || (maxtoks <= 0))
	{
		return 0;
	}

	for(int i = 0; i < maxtoks; i++)
	{
		toks[i] = NULL;
	}

	cursor = s;
	while((*cursor != '\0') && (count < maxtoks))
	{
		cursor += strspn(cursor, delim);
		if(*cursor == '\0')
		{
			break;
		}

		toks[count] = cursor;
		count++;

		cursor += strcspn(cursor, delim);
		if(*cursor != '\0')
		{
			*cursor = '\0';
			cursor++;
		}
	}

	return count;
}
