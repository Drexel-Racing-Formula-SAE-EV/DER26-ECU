/**
* @file rtd_task.c
* @author Cole Bardin (cab572@drexel.edu)
* @brief
* @version 0.1
* @date 2023-09-28
*
* @copyright Copyright (c) 2023
*
*/

#include "tasks/rtd_task.h"
#include "main.h"

#define TRIP_DELAY 100
/**
* @brief Actual RTD task function
*
* @param arg App_data struct pointer converted to void pointer
*/
void rtd_task_fn(void *arg);

TaskHandle_t rtd_task_start(app_data_t *data)
{
   TaskHandle_t handle;
   xTaskCreate(rtd_task_fn, "RTD task", 128, (void *)data, 20, &handle);
   return handle;
}

void rtd_task_fn(void *arg)
{
    app_data_t *data = (app_data_t *)arg;
    uint32_t entry;

	for(;;)
	{
        entry = osKernelGetTickCount();

		data->tsal = HAL_GPIO_ReadPin(TSAL_HV_SIG_GPIO_Port, TSAL_HV_SIG_Pin);
		/* TODO: undo these changes once TSAL is working properly */
		data->tsal = data->board.ams.air_state;
		data->rtd_button = HAL_GPIO_ReadPin(RTD_Go_GPIO_Port, RTD_Go_Pin);
		data->cascadia_ok = !HAL_GPIO_ReadPin(MTR_Ok_GPIO_Port, MTR_Ok_Pin);
		
		// state machine (as described in Teams -> Electrical - Firmware -> Files -> RTD_FSM.pptx)
		switch(data->rtd_mode)
		{
			case RTD_AWAIT_TSAL:
				if(data->tsal)
				{
					data->rtd_mode = RTD_AWAIT_BUTTON_FALSE;
				}
				break;
			
			case RTD_AWAIT_BUTTON_FALSE:
				if(!data->rtd_button)
				{
					data->rtd_mode = RTD_AWAIT_CONDITIONS;
				}

				if(!data->tsal)
				{
					data->rtd_mode = RTD_AWAIT_TSAL;
				}
				break;

			case RTD_AWAIT_CONDITIONS:
				if(data->cascadia_ok && data->brakelight && data->rtd_button)
				{
					set_buzzer(1);
					osDelay(3000);
					set_buzzer(0);
					data->rtd_mode = RTD_ENABLED;
				}

				if(!data->tsal)
				{
					data->rtd_mode = RTD_AWAIT_TSAL;
				}
				break;

			case RTD_ENABLED:
				if(!data->cascadia_ok || !data->rtd_button)
				{
					data->rtd_mode = RTD_AWAIT_CONDITIONS;
				}

				if(!data->tsal)
				{
					data->rtd_mode = RTD_AWAIT_TSAL;
				}

				// for any state transition out of RTD_ENABLE w/o a hard fault
				if (data->rtd_mode != RTD_ENABLED)
				{
					set_ecu_ok(0);
					osDelay(TRIP_DELAY);
					set_ecu_ok(1);

				}

				break;
		}
        osDelayUntil(entry + (1000 / APPS_FREQ));
	}
}
