/**
* @file bse_task.c
* @author Cole Bardin (cab572@drexel.edu)
* @brief
* @version 0.1
* @date 2023-8-15
*
* @copyright Copyright (c) 2023
*
*/

#include "tasks/bse_task.h"
#include "main.h"
#include "ext_drivers/elapsed_fault_timer.h"
#include <math.h>

#define ADC3_MUTEX_TIMEOUT_MS 10u

/**
* @brief Actual BSE task function
*
* @param arg App_data struct pointer converted to void pointer
*/
void bse_task_fn(void *arg);

static StaticTask_t bse_task_tcb;
static StackType_t bse_task_stack[ECU_STACK_BSE_WORDS];
static TaskHandle_t bse_task_handle = NULL;

TaskHandle_t bse_task_start(app_data_t *data)
{
    if(data == NULL)
    {
        return NULL;
    }

    if(bse_task_handle == NULL)
    {
        bse_task_handle = xTaskCreateStatic(bse_task_fn,
            "BSE task", ECU_STACK_BSE_WORDS, (void *)data, BSE_PRIO,
            bse_task_stack, &bse_task_tcb);
    }
    return bse_task_handle;
}

void bse_task_fn(void *arg)
{
    app_data_t *data = (app_data_t *)arg;
    if(data == NULL)
    {
        vTaskDelete(NULL);
        return;
    }
    pressure_sensor_t *bse1 = &data->board.bse1;
    pressure_sensor_t *bse2 = &data->board.bse2;
    float brake_raw;
    bool bse_range_ok;
    bool bse_plausible;
    bool adc_ok;
    uint32_t entry;
    ecu_elapsed_fault_timer_t plausibility_timer = {0};

    for(;;)
    {
        entry = osKernelGetTickCount();
        data->bse_heartbeat_tick = entry;

        if(osMutexAcquire(data->board.stm32f767.adc3_mutex, ADC3_MUTEX_TIMEOUT_MS) == osOK)
        {
            adc_ok = ((stm32f767_adc_switch_channel(bse1->handle, bse1->channel) == HAL_OK) &&
                      (stm32f767_adc_read_checked(bse1->handle, &bse1->count) == HAL_OK) &&
                      (stm32f767_adc_switch_channel(bse2->handle, bse2->channel) == HAL_OK) &&
                      (stm32f767_adc_read_checked(bse2->handle, &bse2->count) == HAL_OK));
            osMutexRelease(data->board.stm32f767.adc3_mutex);
            if(!adc_ok)
            {
                data->bse_fault = true;
                /* Do not publish the last good brake percentage during an
                 * ADC fault. bse_fault remains the validity/torque gate. */
                data->brake = 0;
                set_brakelight(true);
                osDelayUntil(entry + (1000 / BSE_FREQ));
                continue;
            }
        }
        else
        {
            data->bse_fault = true;
            /* A mutex timeout is an invalid sample, not permission to keep
             * replaying stale brake telemetry into BPPC/logging. */
            data->brake = 0;
            set_brakelight(true);
            osDelayUntil(entry + (1000 / BSE_FREQ));
            continue;
        }

        bse_range_ok = (pressure_sensor_in_range(bse1->count, BSE_IMPLAUSIBILITY_MAX, BSE_IMPLAUSIBILITY_MIN) &&
                        pressure_sensor_in_range(bse2->count, BSE_IMPLAUSIBILITY_MAX, BSE_IMPLAUSIBILITY_MIN));

        bse1->percent = pressure_sensor_get_percent(bse1);
        bse2->percent = pressure_sensor_get_percent(bse2);

        /* T.4.3.3 (2022): elapsed-time persistence, never loop count. */
        const bool bse_split =
            fabsf(bse1->percent - bse2->percent) >
            (float)PLAUSIBILITY_THRESH;
        bse_plausible = !ecu_elapsed_fault_timer_update(
            &plausibility_timer, bse_split, entry,
            ECU_BSE_IMPLAUSIBILITY_LIMIT_MS);
        data->bse_fault = (!bse_range_ok || !bse_plausible);

        brake_raw = (bse1->percent + bse2->percent) / 2.0f;
        data->brake = (int)brake_raw;
        set_brakelight(data->bse_fault || (data->brake >= BRAKE_LIGHT_THRESH));

        osDelayUntil(entry + (1000 / BSE_FREQ));
    }
}
