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

#define BV2000_350_PPL 750

/**
* @brief Actual COOL task function
*
* @param arg App_data struct pointer converted to void pointer
*/
void cool_task_fn(void *arg);

bool check_coolant_fault(app_data_t *data);
// Temp sensors: SEN-04-5
float SEN_04_5_convert(uint16_t count);
// Flow sensor: BV2000TRN350B
float BV2000_350_convert(float freq);
// Pressure sensor: Walfront 100PSI pressure transducer 1/8" NPT
float walfront_pressure_convert(float voltage);

TaskHandle_t cool_task_start(app_data_t *data)
{
   TaskHandle_t handle;
   xTaskCreate(cool_task_fn, "COOL task", 128, (void *)data, COOL_PRIO, &handle);
   return handle;
}

void cool_task_fn(void *arg)
{
    app_data_t *data = (app_data_t *)arg;
    pressure_sensor_t *press = &data->board.cool_pressure;
    flow_sensor_t *flow = &data->board.cool_flow;
    ntc_t *temp1 = &data->board.cool_temp1;
    ntc_t *temp2 = &data->board.cool_temp2;
    pwm_t *pump = &data->board.cool_pump;
    float press_voltage;

    uint32_t entry;

	for(;;)
	{
		entry = osKernelGetTickCount();

		// TODO: Finish calibrating coolant sensors
		stm32f767_adc_switch_channel(press->handle, press->channel);
		press->count = stm32f767_adc_read(press->handle);
		press->percent = pressure_sensor_get_percent(press);
		press_voltage = press->count * 3.3 / 4095.0 * 3.0 / 2.0;
		data->coolant_pressure = walfront_pressure_convert(press_voltage);

		data->coolant_flow = BV2000_350_convert(flow->freq);

		stm32f767_adc_switch_channel(temp1->hadc, temp1->channel);
		temp1->count = stm32f767_adc_read(temp1->hadc);
		temp1->temp = SEN_04_5_convert(temp1->count);
		data->coolant_temp_in = temp1->temp;

		stm32f767_adc_switch_channel(temp2->hadc, temp2->channel);
		temp2->count = stm32f767_adc_read(temp2->hadc);
		temp2->temp = SEN_04_5_convert(temp2->count);
		data->coolant_temp_out = temp2->temp;

		// TODO: determine pump speed requirements
		pwm_set_percent(pump, 50);

		data->coolant_fault = check_coolant_fault(data);

		osDelayUntil(entry + (1000 / COOL_FREQ));
	}
}

bool check_coolant_fault(app_data_t *data)
{
	// TODO: Calibrate and check for faults
	return false;
}

float SEN_04_5_convert(uint16_t count)
{
	float voltage = (float)count * 3.3 / 4095.0;
	voltage = voltage * 3.0 / 2.0;

	float temp = voltage; // TODO: replace with real conversion func
	return temp;
}

float BV2000_350_convert(float freq)
{
	return freq * 60.0 / (float)BV2000_350_PPL; // Returns Liters per Minute
}

float walfront_pressure_convert(float voltage)
{
	float press = 25.0 * (voltage - 0.5);
	if(press < 0) return 0;
	if(press > 100) return 100;
	return press;
}
