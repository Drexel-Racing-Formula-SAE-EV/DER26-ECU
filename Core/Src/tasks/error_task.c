/**
 * @file error_task.c
 * @author Cole Bardin (cab572@drexel.edu)
 * @brief
 * @version 0.1
 * @date 2023-09-28
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "main.h"
#include "tasks/error_task.h"

/**
 * @brief Actual ERROR task function
 *
 * @param arg App_data struct pointer converted to void pointer
 */
void error_task_fn(void *arg);

TaskHandle_t error_task_start(app_data_t *data)
{
    TaskHandle_t handle = NULL;

    if(data == NULL)
    {
        return NULL;
    }

    xTaskCreate(error_task_fn, "ERROR task", 128, (void *)data, ERR_PRIO, &handle);
    return handle;
}

void error_task_fn(void *arg)
{
	app_data_t *data = (app_data_t *)arg;
    if(data == NULL)
    {
        vTaskDelete(NULL);
        return;
    }

    uint32_t entry;
    uint32_t cascadia_enable_tick = 0u;

    for(;;)
    {
        entry = osKernelGetTickCount();
        bool cascadia_enable_allowed;

        ams_update_stale(&data->board.ams, entry);
        data->ams_fault = data->board.ams.stale;
        data->canbus_fault = (data->canbus_rx_fault || data->canbus_tx_fault);

        data->cascadia_error = HAL_GPIO_ReadPin(MTR_Fault_GPIO_Port, MTR_Fault_Pin);
		data->imd_fail = HAL_GPIO_ReadPin(IMD_Fail_GPIO_Port, IMD_Fail_Pin);
		data->bms_fail = HAL_GPIO_ReadPin(BMS_Fail_GPIO_Port, BMS_Fail_Pin);
		data->bspd_fail = HAL_GPIO_ReadPin(BSPD_Fail_GPIO_Port, BSPD_Fail_Pin);

		if(!data->board.ams.air_state && !data->imd_fail && !data->bms_fail && !data->bspd_fail) set_ssa(100);
		else set_ssa(0);

//		data->hard_fault = (data->apps_fault ||
//				            data->bse_fault ||
//							data->coolant_fault ||
//							data->cascadia_error
//						    );
		data->hard_fault = (data->coolant_fault ||
							data->cascadia_error
						    );
        
        data->soft_fault =(data->apps_fault ||
        				   data->bse_fault  ||
        				   data->bppc_fault ||
        				   data->cli_fault  ||
						   data->acc_fault  ||
						   data->canbus_fault ||
                           data->ams_fault ||
						   data->dashboard_fault
						   );

        if(data->fw_override) set_ecu_ok(data->fw_override_state);
        else set_ecu_ok(!data->coolant_fault);

        cascadia_enable_allowed = !(data->hard_fault ||
                                    data->imd_fail ||
                                    data->bms_fail ||
                                    data->bspd_fail);
        if(cascadia_enable_allowed)
        {
            if(!data->cascadia_en)
            {
                set_cascadia_on(0);
                set_cascadia_enable(1);
                cascadia_enable_tick = entry;
            }
            else if((uint32_t)(entry - cascadia_enable_tick) >= 3000u)
            {
                set_cascadia_on(1);
            }
        }
        else
        {
            set_cascadia_on(0);
            set_cascadia_enable(0);
            cascadia_enable_tick = entry;
        }

        if(data->hard_fault){
            set_ecu_ok(0);
        }
        osDelayUntil(entry + (1000 / ERR_FREQ));
    }
}
