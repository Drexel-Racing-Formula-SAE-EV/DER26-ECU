/**
 * @file cli.h
 * @author Cole Bardin (cab572@drexel.edu)
 * @brief 
 * @version 0.1
 * @date 2023-10-19
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef ECU_EXT_DRIVERS_CLI_H_
#define ECU_EXT_DRIVERS_CLI_H_

#include <stdint.h>
#include <stdbool.h>

#include "stm32f7xx_hal.h"

#define CLI_LINESZ 256u
#define MAXTOKS (CLI_LINESZ / 2u)

typedef struct {
    uint8_t c;
    unsigned int index;
	UART_HandleTypeDef *huart;
    bool msg_pending;
    unsigned int msg_count;
    unsigned int msg_proc;
    unsigned int msg_valid;
    char line[CLI_LINESZ];
    HAL_StatusTypeDef ret;
} cli_t;

typedef struct {
    const char *name;
    int (*func)(int argc, char *argv[]);
    const char *desc;
} command_t;

void cli_device_init(cli_t *dev, UART_HandleTypeDef *huart);
int cli_printline(cli_t *dev, const char *line);
int tokenize(char *s, char *toks[], int maxtoks, const char *delim);

#endif
