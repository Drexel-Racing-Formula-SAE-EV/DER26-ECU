/**
 * @file acc_task.c
 * @author Cole Bardin (cab572@drexel.edu)
 * @brief
 * @version 0.1
 * @date 2024-02-19
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "main.h"
#include "tasks/acc_task.h"

/**
 * @brief Actual ACC task function
 *
 * @param arg App_data struct pointer converted to void pointer
 */
static void acc_task_fn(void *arg);

TaskHandle_t acc_task_start(app_data_t *data)
{
    TaskHandle_t handle = NULL;

    if(data == NULL)
    {
        return NULL;
    }

    xTaskCreate(acc_task_fn, "ACC task", 128, (void *)data, ACC_PRIO, &handle);
    return handle;
}

static void acc_task_fn(void *arg)
{
    app_data_t *data = (app_data_t *)arg;
    if(data == NULL)
    {
        vTaskDelete(NULL);
        return;
    }
    mpu6050_t *mpu6050 = &data->board.mpu6050;
    HAL_StatusTypeDef ret = HAL_OK;
    uint32_t entry;

    for(;;)
    {
        entry = osKernelGetTickCount();

        ret = mpu6050_read(mpu6050);
        if(ret != HAL_OK) data->acc_fault = true;
        else data->acc_fault = false;

        osDelayUntil(entry + (1000 / ACC_FREQ));
    }
}
