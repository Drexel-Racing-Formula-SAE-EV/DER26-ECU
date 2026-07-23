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
    ecu_cm200_supervisor_t cm200_supervisor = {0};

    ecu_discrete_filter_init(&imd_filter);
    ecu_discrete_filter_init(&bms_filter);
    ecu_discrete_filter_init(&bspd_filter);
    ecu_discrete_filter_init(&mtr_fault_filter);
    ecu_cm200_supervisor_init(&cm200_supervisor);

    for(;;)
    {
        entry = osKernelGetTickCount();
        bool cascadia_enable_allowed;
        bool firmware_ok_allowed;
        bool raw_high;
        bool base_hard_fault;
        bool cm200_feedback_ready;
        bool cm200_torque_ready;
        bool cm200_immediate_fault;
        ecu_fault_inputs_t power_faults;
        ecu_fault_inputs_t shutdown_faults;
        uint32_t comms_now_ms;

        taskENTER_CRITICAL();
        /* Use a timestamp captured while CAN RX is masked.  A timestamp
         * sampled before this critical section can be older than a frame
         * accepted by the ISR immediately before masking, causing one-cycle
         * false stale detection through unsigned age arithmetic. */
        comms_now_ms = HAL_GetTick();
        ams_update_stale(&data->board.ams, comms_now_ms);
        data->ams_fault = !ams_allows_torque(&data->board.ams);
        cm200_update_stale(&data->board.cm200, comms_now_ms);
        cm200_feedback_ready = cm200_feedback_healthy(&data->board.cm200);
        cm200_torque_ready = cm200_allows_torque(&data->board.cm200);
        cm200_immediate_fault = cm200_has_immediate_fault(&data->board.cm200);
        if(data->board.ams.compact_electrical_valid &&
           data->board.ams.compact_electrical_sane &&
           !data->board.ams.compact_electrical_stale &&
           data->board.cm200.frame[CM200_FRAME_VOLTAGE].valid &&
           data->board.cm200.frame[CM200_FRAME_VOLTAGE].sane &&
           !data->board.cm200.frame[CM200_FRAME_VOLTAGE].stale)
        {
            int32_t voltage_delta = (int32_t)data->board.cm200.dc_bus_voltage_0p1v -
                                    (int32_t)data->board.ams.pack_voltage_0p1v;
            data->ams_cm200_voltage_delta_0p1v = (int16_t)voltage_delta;
            data->ams_cm200_voltage_crosscheck_valid = true;
            data->ams_cm200_voltage_mismatch =
                ((voltage_delta > (int32_t)ECU_AMS_CM200_VOLTAGE_TOLERANCE_0P1V) ||
                 (voltage_delta < -(int32_t)ECU_AMS_CM200_VOLTAGE_TOLERANCE_0P1V));
        }
        else
        {
            data->ams_cm200_voltage_delta_0p1v = 0;
            data->ams_cm200_voltage_crosscheck_valid = false;
            data->ams_cm200_voltage_mismatch = false;
        }
        taskEXIT_CRITICAL();
        data->cm200_ready = cm200_torque_ready;
        if((uint32_t)(entry - supervisor_start_tick) >= 1000u)
        {
            data->task_heartbeat_fault =
                ecu_heartbeat_expired(data->apps_heartbeat_tick, entry, 250u) ||
                ecu_heartbeat_expired(data->bse_heartbeat_tick, entry, 250u) ||
                ecu_heartbeat_expired(data->bppc_heartbeat_tick, entry, 250u) ||
                ecu_heartbeat_expired(data->rtd_heartbeat_tick, entry, 250u) ||
                ecu_heartbeat_expired(data->can_heartbeat_tick, entry, 250u) ||
                ecu_heartbeat_expired(data->cool_heartbeat_tick, entry, 750u);
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
        base_hard_fault = (data->coolant_fault ||
                           data->cascadia_error ||
                           data->startup_fault ||
                           data->task_heartbeat_fault);

        /* Missing CM200 feedback cannot prevent the ECU from initially
         * powering the controller, because the broadcasts may not exist until
         * Cascadia_ON is asserted.  A latched CM200 startup/runtime fault does
         * prevent another power attempt until MCU reset. */
        power_faults = (ecu_fault_inputs_t){
            .hard_fault = base_hard_fault,
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
            .cm200_fault = false,
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

        cascadia_enable_allowed = (!ECU_OUTPUTS_INHIBITED &&
                                   !cm200_supervisor.startup_timeout_latched &&
                                   !cm200_supervisor.runtime_fault_latched &&
                                   ecu_faults_clear(&power_faults));
        if(cascadia_enable_allowed)
        {
            if(!data->cascadia_en)
            {
                set_cascadia_on(0);
                set_cascadia_enable(1);
                cascadia_enable_tick = entry;
            }
            else if((uint32_t)(entry - cascadia_enable_tick) >=
                    ECU_CM200_POWER_SEQUENCE_DELAY_MS)
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

        data->cm200_fault = ecu_cm200_supervisor_update(&cm200_supervisor,
                                                        data->cascadia_on,
                                                        cm200_feedback_ready,
                                                        cm200_immediate_fault,
                                                        entry);
        data->cm200_feedback_seen = cm200_supervisor.ever_ready;
        data->cm200_startup_timeout = cm200_supervisor.startup_timeout_latched;
        data->cm200_runtime_fault_latched = cm200_supervisor.runtime_fault_latched;

        if(data->cm200_startup_timeout || data->cm200_runtime_fault_latched)
        {
            set_cascadia_on(0);
            set_cascadia_enable(0);
        }

        data->hard_fault = (base_hard_fault ||
                            data->cm200_startup_timeout ||
                            data->cm200_runtime_fault_latched);
        data->soft_fault = (data->apps_fault ||
                            data->bse_fault ||
                            data->bppc_fault ||
                            data->cli_fault ||
                            data->acc_fault ||
                            data->canbus_fault ||
                            data->ams_fault ||
                            data->cm200_fault ||
                            data->dashboard_fault);

        shutdown_faults = power_faults;
        shutdown_faults.hard_fault = data->hard_fault;
        shutdown_faults.cm200_fault = data->cm200_fault;

        firmware_ok_allowed = (!ECU_OUTPUTS_INHIBITED &&
                               !rtd_trip_pulse_active &&
                               ecu_faults_clear(&shutdown_faults));
        set_ecu_ok(firmware_ok_allowed);

        if(!data->startup_fault && !data->task_heartbeat_fault)
        {
            ecu_watchdog_refresh();
        }
        osDelayUntil(entry + (1000 / ERR_FREQ));
    }
}
