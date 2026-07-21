/**
* @file cool_task.c
* @author Cole Bardin (cab572@drexel.edu)
* @brief
* @version 0.1
* @date 2024-3-26
*
* @copyright Copyright (c) 2023
*
*/

#include "tasks/cool_task.h"
#include "main.h"
#include "ecu_config.h"

#include <math.h>

#define BV2000_350_PPL 750
#define ADC3_MUTEX_TIMEOUT_MS 10u
#define COOLANT_FLOW_STALE_TIMEOUT_MS 1000u

/**
* @brief Actual COOL task function
*
* @param arg App_data struct pointer converted to void pointer
*/
void cool_task_fn(void *arg);

bool check_coolant_fault(app_data_t *data);
/* Temp sensors: SEN-04-5 */
float SEN_04_5_convert(uint16_t count);
/* Flow sensor: BV2000TRN350B */
float BV2000_350_convert(float freq);
/* Pressure sensor: Walfront 100PSI pressure transducer 1/8" NPT */
float walfront_pressure_convert(float voltage);

TaskHandle_t cool_task_start(app_data_t *data)
{
   TaskHandle_t handle = NULL;

   if(data == NULL)
   {
       return NULL;
   }

   xTaskCreate(cool_task_fn, "COOL task", 256, (void *)data, COOL_PRIO, &handle);
   return handle;
}

void cool_task_fn(void *arg)
{
    app_data_t *data = (app_data_t *)arg;
    if(data == NULL)
    {
        vTaskDelete(NULL);
        return;
    }
    pressure_sensor_t *press = &data->board.cool_pressure;
    flow_sensor_t *flow = &data->board.cool_flow;
    ntc_t *temp1 = &data->board.cool_temp1;
    ntc_t *temp2 = &data->board.cool_temp2;
    pwm_t *pump = &data->board.cool_pump;
    float press_voltage;

    uint32_t entry;
    uint8_t adc_mutex_failures = 0u;
    bool adc_ok;

    for(;;)
    {
        entry = osKernelGetTickCount();
        data->cool_heartbeat_tick = entry;

        /* TODO: Finish calibrating coolant sensors. ADC3 is shared with brake sensors, so this block is mutexed. */
        if(osMutexAcquire(data->board.stm32f767.adc3_mutex, ADC3_MUTEX_TIMEOUT_MS) == osOK)
        {
            adc_ok = ((stm32f767_adc_switch_channel(press->handle, press->channel) == HAL_OK) &&
                      (stm32f767_adc_read_checked(press->handle, &press->count) == HAL_OK));
            press->percent = pressure_sensor_get_percent(press);
            press_voltage = press->count * 3.3f / 4095.0f * 3.0f / 2.0f;
            data->coolant_pressure = walfront_pressure_convert(press_voltage);

            adc_ok = (adc_ok &&
                      (stm32f767_adc_switch_channel(temp1->hadc, temp1->channel) == HAL_OK) &&
                      (stm32f767_adc_read_checked(temp1->hadc, &temp1->count) == HAL_OK));
            temp1->temp = SEN_04_5_convert(temp1->count);
            data->coolant_temp_in = temp1->temp;

            adc_ok = (adc_ok &&
                      (stm32f767_adc_switch_channel(temp2->hadc, temp2->channel) == HAL_OK) &&
                      (stm32f767_adc_read_checked(temp2->hadc, &temp2->count) == HAL_OK));
            temp2->temp = SEN_04_5_convert(temp2->count);
            data->coolant_temp_out = temp2->temp;

            /* Temperature transfer function is not yet calibrated.  Keep the
             * measurements visible as invalid instead of labelling volts as C. */
            data->coolant_telemetry_valid = false;

            osMutexRelease(data->board.stm32f767.adc3_mutex);
            if(adc_ok)
            {
                adc_mutex_failures = 0u;
                data->coolant_fault = false;
            }
            else
            {
                if(adc_mutex_failures < 3u)
                {
                    adc_mutex_failures++;
                }
                data->coolant_fault = (adc_mutex_failures >= 3u);
            }
        }
        else
        {
            if(adc_mutex_failures < 3u)
            {
                adc_mutex_failures++;
            }
            data->coolant_fault = (adc_mutex_failures >= 3u);
        }

        taskENTER_CRITICAL();
        flow_sensor_update_stale(flow, entry, COOLANT_FLOW_STALE_TIMEOUT_MS);
        const float flow_frequency = flow->freq;
        taskEXIT_CRITICAL();
        data->coolant_flow = BV2000_350_convert(flow_frequency);

        /* Fail toward maximum coolant circulation until control calibration is closed. */
        pwm_set_percent(pump, ECU_COOLANT_PUMP_DEFAULT_PERCENT);

        /* data->coolant_fault = check_coolant_fault(data); */

        osDelayUntil(entry + (1000 / COOL_FREQ));
    }
}

bool check_coolant_fault(app_data_t *data)
{
    /* TODO: Calibrate and check for faults. */
    (void)data;
    /* if(data->coolant_flow < COOLANT_FLOW_MIN) return true; */
    return false;
}

float SEN_04_5_convert(uint16_t count)
{
    (void)count;
    return NAN;
}

float BV2000_350_convert(float freq)
{
    return freq * 60.0f / (float)BV2000_350_PPL; /* Returns Liters per Minute. */
}

float walfront_pressure_convert(float voltage)
{
    float press = 25.0f * (voltage - 0.5f);
    if(press < 0.0f)
    {
        return 0.0f;
    }
    if(press > 100.0f)
    {
        return 100.0f;
    }
    return press;
}
