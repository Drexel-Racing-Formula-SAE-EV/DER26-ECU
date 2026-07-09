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

#define TRIP_DELAY 100u
#define RTD_BUZZ_TIME_MS 3000u

/**
* @brief Actual RTD task function
*
* @param arg App_data struct pointer converted to void pointer
*/
void rtd_task_fn(void *arg);

static bool rtd_faults_clear(const app_data_t *data)
{
    return !(data->hard_fault ||
             data->apps_fault ||
             data->bse_fault ||
             data->bppc_fault ||
             data->ams_fault ||
             data->canbus_fault ||
             data->canbus_rx_fault ||
             data->canbus_tx_fault ||
             data->imd_fail ||
             data->bms_fail ||
             data->bspd_fail);
}

static bool rtd_conditions_met(const app_data_t *data)
{
    return (data->tsal &&
            data->cascadia_ok &&
            data->brakelight &&
            data->rtd_button &&
            rtd_faults_clear(data));
}

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

        /* state machine (as described in Teams -> Electrical - Firmware -> Files -> RTD_FSM.pptx) */
        switch(data->rtd_mode)
        {
            case RTD_AWAIT_TSAL:
                set_buzzer(false);
                if(data->tsal)
                {
                    data->rtd_mode = RTD_AWAIT_BUTTON_FALSE;
                }
                break;

            case RTD_AWAIT_BUTTON_FALSE:
                set_buzzer(false);
                if(!data->tsal)
                {
                    data->rtd_mode = RTD_AWAIT_TSAL;
                }
                else if(!data->rtd_button)
                {
                    data->rtd_mode = RTD_AWAIT_CONDITIONS;
                }
                break;

            case RTD_AWAIT_CONDITIONS:
                set_buzzer(false);
                if(!data->tsal)
                {
                    data->rtd_mode = RTD_AWAIT_TSAL;
                }
                else if(rtd_conditions_met(data))
                {
                    set_buzzer(true);
                    buzz_start_tick = entry;
                    data->rtd_mode = RTD_BUZZING;
                }
                break;

            case RTD_BUZZING:
                if(!data->tsal)
                {
                    set_buzzer(false);
                    data->rtd_mode = RTD_AWAIT_TSAL;
                }
                else if(!rtd_conditions_met(data))
                {
                    set_buzzer(false);
                    data->rtd_mode = RTD_AWAIT_CONDITIONS;
                }
                else if((uint32_t)(entry - buzz_start_tick) >= RTD_BUZZ_TIME_MS)
                {
                    set_buzzer(false);
                    data->rtd_mode = RTD_ENABLED;
                }
                break;

            case RTD_ENABLED:
                if(!data->tsal)
                {
                    data->rtd_mode = RTD_AWAIT_TSAL;
                }
                else if(!data->cascadia_ok || !data->rtd_button || !rtd_faults_clear(data))
                {
                    data->rtd_mode = RTD_AWAIT_CONDITIONS;
                }

                /* For any state transition out of RTD_ENABLED without a hard fault. */
                if(data->rtd_mode != RTD_ENABLED)
                {
                    set_buzzer(false);
                    override_ecu_ok(false);
                    apply_ecu_ok_override(true);
                    osDelay(TRIP_DELAY);
                    apply_ecu_ok_override(false);
                }
                break;

            default:
                set_buzzer(false);
                data->rtd_mode = RTD_AWAIT_TSAL;
                break;
        }
        osDelayUntil(entry + (1000 / RTD_FREQ));
    }
}
