/**
* @file log_task.c
* @author Alex Pylaras (ap3782@drexel.edu)
* @brief
* @version 0.1
* @date 2026-01-08
*
* @copyright Copyright (c) 2026
*
*/

#include "tasks/log_task.h"
#include "main.h"
#include "fatfs.h"
#include <stdio.h>

/**
* @brief Actual Log task function
*
* @param arg App_data struct pointer converted to void pointer
*/
void log_task_fn(void *arg);

TaskHandle_t log_task_start(app_data_t *data)
{
   TaskHandle_t handle;
   xTaskCreate(log_task_fn, "Log task", 1024, (void *)data, LOG_PRIO, &handle);
   return handle;
}

void log_task_fn(void *arg)
{
    app_data_t *data = (app_data_t *)arg;
    uint32_t entry;

    static FATFS FatFs;
    static bool fs_mounted = false;

    // Need to give the SD card a moment to settle, otherwise you'll get FR_NOT_READY when mounting
    entry = osKernelGetTickCount();
    osDelayUntil(entry + (1000 / LOG_FREQ));

	// Initial mount
    if (f_mount(&FatFs, "", 1) == FR_OK)
    {
        fs_mounted = true;
    }
    else
    {
        data->log_fault = true;
    }

    for (;;)
    {
        entry = osKernelGetTickCount();

        if (fs_mounted)
        {
            FIL fil;
            FRESULT fres;

            fres = f_open(&fil, "der26.log", FA_WRITE | FA_OPEN_APPEND);
            if (fres == FR_OK)
            {
                char line[80];
                read_time();
                int len = snprintf(
                    line,
                    sizeof(line),
                    "%04u-%02u-%02u %02u:%02u:%02u | hard_fault=%s | soft_fault=%s\r\n",
                    data->datetime.year,
                    data->datetime.month,
                    data->datetime.day,
                    data->datetime.hour,
                    data->datetime.minute,
                    data->datetime.second,
                    data->hard_fault ? "true" : "false",
                    data->soft_fault ? "true" : "false"
                );

                if (len > 0 && len < sizeof(line))
                {
                	UINT bw;
                    f_write(&fil, line, len, &bw);
                    if (bw < len) {  // Make sure the the SD card didn't run out of space
                    	data->log_fault = true;
                    }
                    f_sync(&fil);
                }

                f_close(&fil);
            }
        }

        osDelayUntil(entry + (1000 / LOG_FREQ));
    }
}
