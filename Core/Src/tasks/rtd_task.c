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
#include "ext_drivers/ecu_safety.h"

#define TRIP_DELAY 100u

/**
* @brief Actual RTD task function
*
* @param arg App_data struct pointer converted to void pointer
*/
void rtd_task_fn(void *arg);

TaskHandle_t rtd_task_start(app_data_t *data)
{
   TaskHandle_t handle = NULL;

   if(data == NULL)
   {
       return NULL;
   }

   xTaskCreate(rtd_task_fn, "RTD task", 256, (void *)data, RTD_PRIO, &handle);
   return handle;
}

void rtd_task_fn(void *arg)
{
    app_data_t *data = (app_data_t *)arg;
    if(data == NULL)
    {
        vTaskDelete(NULL);
        return;
    }
    uint32_t entry;
    uint32_t buzz_start_tick = 0u;

    for(;;)
    {
        entry = osKernelGetTickCount();

        data->tsal = HAL_GPIO_ReadPin(TSAL_HV_SIG_GPIO_Port, TSAL_HV_SIG_Pin);
        data->rtd_button = !HAL_GPIO_ReadPin(RTD_Go_GPIO_Port, RTD_Go_Pin);
        data->cascadia_ok = !HAL_GPIO_ReadPin(MTR_Ok_GPIO_Port, MTR_Ok_Pin);

        const ecu_rtd_inputs_t inputs = {
            .tsal = data->tsal,
            .rtd_button = data->rtd_button,
            .cascadia_ok = data->cascadia_ok,
            .brakelight = data->brakelight,
            .faults = {
                .hard_fault = data->hard_fault,
                .apps_fault = data->apps_fault,
                .bse_fault = data->bse_fault,
                .bppc_fault = data->bppc_fault,
                .ams_fault = data->ams_fault,
                .canbus_fault = data->canbus_fault,
                .canbus_rx_fault = data->canbus_rx_fault,
                .canbus_tx_fault = data->canbus_tx_fault,
                .imd_fail = data->imd_fail,
                .bms_fail = data->bms_fail,
                .bspd_fail = data->bspd_fail,
            },
        };

        const ecu_rtd_step_t step = ecu_rtd_step(data->rtd_mode, buzz_start_tick, &inputs, entry);
        data->rtd_mode = step.state;
        buzz_start_tick = step.buzz_start_tick;
        set_buzzer(step.buzzer_on);

        if(step.trip_pulse_requested)
        {
            override_ecu_ok(false);
            apply_ecu_ok_override(true);
            osDelay(TRIP_DELAY);
            apply_ecu_ok_override(false);
        }

        osDelayUntil(entry + (1000 / RTD_FREQ));
    }
}
