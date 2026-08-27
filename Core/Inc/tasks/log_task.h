/**
* @file log_task.h
* @author Alex Pylaras (ap3782@drexel.edu)
* @brief
* @version 0.1
* @date 2026-01-08
*
* @copyright Copyright (c) 2026
*
*/

#ifndef __LOG_TASK_H_
#define __LOG_TASK_H_

#include "app.h"

#include "cmsis_os.h"

/**
* @brief Starts the Log task
*
* @param data App data structure pointer
* @return TaskHandle_t Handle used for task
*/
TaskHandle_t log_task_start(app_data_t *data);

#endif
