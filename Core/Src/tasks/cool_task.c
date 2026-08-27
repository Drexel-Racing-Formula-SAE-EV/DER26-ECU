/**
 * @file cool_task.c
 * @brief DER26 coolant acquisition, plausibility and fail-safe pump control.
 */
#include "tasks/cool_task.h"
#include "main.h"
#include "ecu_config.h"
#include "ext_drivers/cooling_control.h"

#include <math.h>

#define ADC3_MUTEX_TIMEOUT_MS 10u
#define COOLANT_FLOW_STALE_TIMEOUT_MS 1000u

void cool_task_fn(void *arg);

static StaticTask_t cool_task_tcb;
static StackType_t cool_task_stack[ECU_STACK_COOL_WORDS];
static TaskHandle_t cool_task_handle = NULL;

/* Retained public helpers for existing bench code. */
bool check_coolant_fault(app_data_t *data);
float SEN_04_5_convert(uint16_t count);
float BV2000_350_convert(float freq);
float walfront_pressure_convert(float voltage);

TaskHandle_t cool_task_start(app_data_t *data)
{
    if(data == NULL) return NULL;
    if(cool_task_handle == NULL)
    {
        cool_task_handle = xTaskCreateStatic(cool_task_fn,
            "COOL task", ECU_STACK_COOL_WORDS, (void *)data, COOL_PRIO,
            cool_task_stack, &cool_task_tcb);
    }
    return cool_task_handle;
}

#if ECU_COOLING_VALIDATED
static float cooling_auto_percent(const ecu_cooling_sample_t *sample,
                                  bool fault)
{
    if((sample == NULL) || fault || !sample->temp_in_valid ||
       !sample->temp_out_valid)
    {
        return NAN;
    }

    const float maximum_c = (sample->temp_in_c > sample->temp_out_c) ?
                            sample->temp_in_c : sample->temp_out_c;
    if(maximum_c <= 35.0f) return 25.0f;
    if(maximum_c >= 70.0f) return 100.0f;
    return 25.0f + ((maximum_c - 35.0f) * (75.0f / 35.0f));
}
#endif

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
    ecu_cooling_monitor_t monitor;
    ecu_cooling_monitor_init(&monitor);

    uint8_t adc_failures = 0u;
    for(;;)
    {
        const uint32_t entry = osKernelGetTickCount();
        data->cool_heartbeat_tick = entry;

        ecu_cooling_sample_t sample = {0};
        bool adc_ok = false;
        if(osMutexAcquire(data->board.stm32f767.adc3_mutex,
                          ADC3_MUTEX_TIMEOUT_MS) == osOK)
        {
            const bool pressure_read =
                (stm32f767_adc_switch_channel(press->handle, press->channel) == HAL_OK) &&
                (stm32f767_adc_read_checked(press->handle, &press->count) == HAL_OK);
            const bool temp1_read =
                (stm32f767_adc_switch_channel(temp1->hadc, temp1->channel) == HAL_OK) &&
                (stm32f767_adc_read_checked(temp1->hadc, &temp1->count) == HAL_OK);
            const bool temp2_read =
                (stm32f767_adc_switch_channel(temp2->hadc, temp2->channel) == HAL_OK) &&
                (stm32f767_adc_read_checked(temp2->hadc, &temp2->count) == HAL_OK);
            osMutexRelease(data->board.stm32f767.adc3_mutex);

            sample.pressure_psi = ecu_coolant_pressure_100psi_from_adc(
                press->count, &sample.pressure_valid);
            sample.temp_in_c = ecu_coolant_temp_sen04_5_from_adc(
                temp1->count, &sample.temp_in_valid);
            sample.temp_out_c = ecu_coolant_temp_sen04_5_from_adc(
                temp2->count, &sample.temp_out_valid);
            sample.pressure_valid = sample.pressure_valid && pressure_read;
            sample.temp_in_valid = sample.temp_in_valid && temp1_read;
            sample.temp_out_valid = sample.temp_out_valid && temp2_read;
            adc_ok = pressure_read && temp1_read && temp2_read;
        }

        if(adc_ok)
        {
            adc_failures = 0u;
        }
        else if(adc_failures < UINT8_MAX)
        {
            adc_failures++;
        }
        if(adc_failures >= 3u)
        {
            sample.pressure_valid = false;
            sample.temp_in_valid = false;
            sample.temp_out_valid = false;
        }

        taskENTER_CRITICAL();
        flow_sensor_update_stale(flow, entry, COOLANT_FLOW_STALE_TIMEOUT_MS);
        const float flow_frequency = flow->freq;
        const bool flow_input_valid = flow->valid && !flow->stale;
        taskEXIT_CRITICAL();
        sample.flow_lpm = ecu_coolant_flow_bv2000_from_hz(
            flow_frequency, flow_input_valid, &sample.flow_valid);

        uint8_t pump_mode;
        float manual_percent;
        taskENTER_CRITICAL();
        pump_mode = data->coolant_pump_mode;
        manual_percent = data->coolant_pump_manual_percent;
        taskEXIT_CRITICAL();

        bool manual = false;
        bool failsafe_max = false;
        float requested_pct = 100.0f;
        if(pump_mode == 1u)
        {
            failsafe_max = true;
        }
        else if(pump_mode == 2u)
        {
            manual = true;
            requested_pct = manual_percent;
        }
        else
        {
            /* Until assembled-loop validation closes ECU_COOLING_VALIDATED,
             * AUTO intentionally uses the pump's full-speed fail-safe. */
#if ECU_COOLING_VALIDATED
            requested_pct = cooling_auto_percent(&sample, monitor.fault);
            failsafe_max = !isfinite(requested_pct);
#else
            failsafe_max = true;
#endif
        }

        ecu_coolant_pump_command_t pump_command =
            ecu_coolant_pump_command(requested_pct, failsafe_max, manual);
        if(pwm_set_percent(pump, pump_command.mcu_gate_duty_pct) != 0)
        {
            pump_command = ecu_coolant_pump_command(NAN, true, manual);
            (void)pwm_set_percent(pump, 0.0f);
        }
        sample.pump_command_pct = pump_command.requested_pct;

        const bool cooling_fault =
            ecu_cooling_monitor_update(&monitor, &sample);
        const bool telemetry_valid = sample.temp_in_valid &&
                                     sample.temp_out_valid &&
                                     sample.pressure_valid &&
                                     sample.flow_valid;

        taskENTER_CRITICAL();
        press->percent = sample.pressure_valid ?
            pressure_sensor_get_percent(press) : 0.0f;
        temp1->temp = sample.temp_in_c;
        temp2->temp = sample.temp_out_c;
        data->coolant_pressure = sample.pressure_psi;
        data->coolant_temp_in = sample.temp_in_c;
        data->coolant_temp_out = sample.temp_out_c;
        data->coolant_flow = sample.flow_lpm;
        data->coolant_telemetry_valid = telemetry_valid;
        data->coolant_fault = cooling_fault;
        data->coolant_fault_flags = monitor.fault_flags;
        data->coolant_pump_command_percent = pump_command.requested_pct;
        data->coolant_pump_s_duty_percent = pump_command.pump_s_duty_pct;
        data->coolant_pump_gate_duty_percent = pump_command.mcu_gate_duty_pct;
        data->coolant_pump_flags = pump_command.flags;
        taskEXIT_CRITICAL();

        osDelayUntil(entry + (1000u / COOL_FREQ));
    }
}

bool check_coolant_fault(app_data_t *data)
{
    return (data == NULL) || data->coolant_fault;
}

float SEN_04_5_convert(uint16_t count)
{
    bool valid;
    return ecu_coolant_temp_sen04_5_from_adc(count, &valid);
}

float BV2000_350_convert(float freq)
{
    bool valid;
    return ecu_coolant_flow_bv2000_from_hz(freq, true, &valid);
}

float walfront_pressure_convert(float voltage)
{
    if(!isfinite(voltage)) return NAN;
    float pressure = 25.0f * (voltage - 0.5f);
    if(pressure < 0.0f) pressure = 0.0f;
    if(pressure > 100.0f) pressure = 100.0f;
    return pressure;
}
