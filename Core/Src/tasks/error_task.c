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
#include "ecu_config.h"
#include "ext_drivers/ecu_safety.h"

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

    xTaskCreate(error_task_fn, "ERROR task", 256, (void *)data, ERR_PRIO, &handle);
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
    uint32_t rtd_trip_pulse_start = 0u;
    bool rtd_trip_pulse_active = false;
    uint32_t supervisor_start_tick = osKernelGetTickCount();
    ecu_discrete_filter_t imd_filter = {0};
    ecu_discrete_filter_t bms_filter = {0};
    ecu_discrete_filter_t bspd_filter = {0};
    ecu_discrete_filter_t mtr_fault_filter = {0};

    ecu_discrete_filter_init(&imd_filter);
    ecu_discrete_filter_init(&bms_filter);
    ecu_discrete_filter_init(&bspd_filter);
    ecu_discrete_filter_init(&mtr_fault_filter);

    for(;;)
    {
        entry = osKernelGetTickCount();
        bool cascadia_enable_allowed;
        bool firmware_ok_allowed;
        bool raw_high;
        ecu_fault_inputs_t shutdown_faults;

        taskENTER_CRITICAL();
        ams_update_stale(&data->board.ams, entry);
        data->ams_fault = !ams_allows_torque(&data->board.ams);
        taskEXIT_CRITICAL();
        if((uint32_t)(entry - supervisor_start_tick) >= 1000u)
        {
            data->task_heartbeat_fault =
                ecu_heartbeat_expired(data->apps_heartbeat_tick, entry, 250u) ||
                ecu_heartbeat_expired(data->bse_heartbeat_tick, entry, 250u) ||
                ecu_heartbeat_expired(data->bppc_heartbeat_tick, entry, 250u) ||
                ecu_heartbeat_expired(data->rtd_heartbeat_tick, entry, 500u) ||
                ecu_heartbeat_expired(data->can_heartbeat_tick, entry, 250u);
        }
        data->canbus_fault = (data->canbus_rx_fault || data->canbus_tx_fault || data->canbus_hw_fault);

        raw_high = (HAL_GPIO_ReadPin(MTR_Fault_GPIO_Port, MTR_Fault_Pin) == GPIO_PIN_SET);
        data->cascadia_error = ecu_discrete_fault_update(&mtr_fault_filter,
                                                         ECU_MTR_FAULT_ACTIVE_HIGH ? raw_high : !raw_high,
                                                         ECU_DISCRETE_CLEAR_SAMPLES);

        raw_high = (HAL_GPIO_ReadPin(IMD_Fail_GPIO_Port, IMD_Fail_Pin) == GPIO_PIN_SET);
		data->imd_fail = ecu_discrete_fault_update(&imd_filter,
                                                    ECU_IMD_FAIL_ACTIVE_HIGH ? raw_high : !raw_high,
                                                    ECU_DISCRETE_CLEAR_SAMPLES);

        raw_high = (HAL_GPIO_ReadPin(BMS_Fail_GPIO_Port, BMS_Fail_Pin) == GPIO_PIN_SET);
		data->bms_fail = ecu_discrete_fault_update(&bms_filter,
                                                    ECU_BMS_FAIL_ACTIVE_HIGH ? raw_high : !raw_high,
                                                    ECU_DISCRETE_CLEAR_SAMPLES);

        data->bspd_ok_raw = (HAL_GPIO_ReadPin(BSPD_OK_GPIO_Port, BSPD_OK_Pin) == GPIO_PIN_SET);
		data->bspd_fail = ecu_discrete_fault_update(&bspd_filter,
                                                     ecu_bspd_raw_is_fault(data->bspd_ok_raw),
                                                     ECU_DISCRETE_CLEAR_SAMPLES);

		if(!data->tsal && !data->imd_fail && !data->bms_fail && !data->bspd_fail) set_ssa(100);
		else set_ssa(0);

//		data->hard_fault = (data->apps_fault ||
//				            data->bse_fault ||
//							data->coolant_fault ||
//							data->cascadia_error
//						    );
		data->hard_fault = (data->coolant_fault ||
							data->cascadia_error ||
                            data->startup_fault ||
                            data->task_heartbeat_fault
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


        shutdown_faults = (ecu_fault_inputs_t){
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
        };

        if(data->rtd_trip_pulse_requested)
        {
            data->rtd_trip_pulse_requested = false;
            rtd_trip_pulse_active = true;
            rtd_trip_pulse_start = entry;
        }
        if(rtd_trip_pulse_active &&
           ((uint32_t)(entry - rtd_trip_pulse_start) >= ECU_RTD_TRIP_PULSE_MS))
        {
            rtd_trip_pulse_active = false;
        }

        firmware_ok_allowed = (!ECU_OUTPUTS_INHIBITED &&
                               !rtd_trip_pulse_active &&
                               ecu_faults_clear(&shutdown_faults));
        set_ecu_ok(firmware_ok_allowed);

        cascadia_enable_allowed = (!ECU_OUTPUTS_INHIBITED &&
                                   ecu_faults_clear(&shutdown_faults));
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

        if(!data->startup_fault && !data->task_heartbeat_fault)
        {
            ecu_watchdog_refresh();
        }
        osDelayUntil(entry + (1000 / ERR_FREQ));
    }
}
