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
#include "ecu_config.h"
#include "ext_drivers/ecu_safety.h"

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
    ecu_discrete_filter_t tsal_filter = {0};
    ecu_discrete_filter_t motor_ok_filter = {0};

    ecu_discrete_filter_init(&tsal_filter);
    ecu_discrete_filter_init(&motor_ok_filter);

    for(;;)
    {
        entry = osKernelGetTickCount();
        data->rtd_heartbeat_tick = entry;

        const bool tsal_raw = (HAL_GPIO_ReadPin(TSAL_HV_SIG_GPIO_Port, TSAL_HV_SIG_Pin) == GPIO_PIN_SET);
        const bool button_raw = (HAL_GPIO_ReadPin(RTD_Go_GPIO_Port, RTD_Go_Pin) == GPIO_PIN_SET);
        const bool mtr_ok_raw = (HAL_GPIO_ReadPin(MTR_Ok_GPIO_Port, MTR_Ok_Pin) == GPIO_PIN_SET);
        const bool tsal_decoded = ECU_TSAL_ACTIVE_HIGH ? tsal_raw : !tsal_raw;
        const bool motor_ok_decoded = ECU_MTR_OK_ACTIVE_LOW ? !mtr_ok_raw : mtr_ok_raw;
        /* Loss is immediate; assertion requires three consecutive 50 Hz
         * samples so startup/glitch edges cannot arm RTD. */
        data->tsal = !ecu_discrete_fault_update(&tsal_filter,
                                                 !tsal_decoded,
                                                 3u);
        data->rtd_button = ECU_RTD_BUTTON_ACTIVE_LOW ? !button_raw : button_raw;
        data->cascadia_ok = !ecu_discrete_fault_update(&motor_ok_filter,
                                                        !motor_ok_decoded,
                                                        3u);

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
                .cm200_fault = (data->cm200_fault || !data->cm200_ready),
            },
        };

        const ecu_rtd_step_t step = ecu_rtd_step(data->rtd_mode, buzz_start_tick, &inputs, entry);
        data->rtd_mode = step.state;
        buzz_start_tick = step.buzz_start_tick;
        set_buzzer(step.buzzer_on);

        if(step.trip_pulse_requested)
        {
            /* ERROR task is the sole owner of shutdown outputs. */
            data->rtd_trip_pulse_requested = true;
        }

        osDelayUntil(entry + (1000 / RTD_FREQ));
    }
}
