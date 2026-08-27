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

void cli_device_init(cli_t *dev, UART_HandleTypeDef *huart)
{
	if(dev == NULL)
	{
		return;
	}
	dev->huart = huart;
	dev->c = 0u;
	dev->index = 0;
    dev->msg_pending = false;
    dev->msg_count = 0;
    dev->msg_proc = 0;
	dev->msg_valid = 0;
	dev->ret = 0;
	memset(dev->line, 0, sizeof(dev->line));
}

int cli_printline(cli_t *dev, char *line)
{
	static char nl[] = "\r\n";
	HAL_StatusTypeDef ret = 0;

	if((dev == NULL) || (line == NULL) || (dev->huart == NULL))
	{
		return HAL_ERROR;
	}

	if(xPortIsInsideInterrupt())
	{
		return HAL_BUSY;
	}

	ret |= HAL_UART_Transmit(dev->huart, (uint8_t *)line, strlen(line), 100);
	ret |= HAL_UART_Transmit(dev->huart, (uint8_t *)nl, strlen(nl), 100);
	return ret;
}

int tokenize(char *s, char *toks[], int maxtoks, char *delim)
{
	int count = 0;
	char *tok;

	if((s == NULL) || (toks == NULL) || (delim == NULL) || (maxtoks <= 0))
	{
		return 0;
	}

	tok = strtok(s, delim);
	while((tok != NULL) && (count < (maxtoks - 1)))
	{
		toks[count++] = tok;
		tok = strtok(NULL, delim);
	}
	toks[count] = NULL;
	return count;
}
