/*
 * ecu_data_logger.h
 * Non-safety SD/CAN logger for DER26 ECU bench and vehicle data capture.
 */
#ifndef INC_EXT_DRIVERS_ECU_DATA_LOGGER_H_
#define INC_EXT_DRIVERS_ECU_DATA_LOGGER_H_

#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

typedef struct
{
    bool enabled;
    bool active;
    bool raw_enabled;
    bool files_open;
    bool auto_start;
    uint16_t session_index;
    uint16_t rate_hz;
    uint32_t start_tick;
    uint32_t decoded_rows;
    uint32_t raw_records;
    uint32_t event_rows;
    uint32_t raw_dropped;
    uint32_t raw_high_water;
    uint32_t write_errors;
    uint32_t auto_stop_count;
    uint16_t consecutive_error_cycles;
    uint32_t sync_count;
    uint32_t last_sync_tick;
    uint32_t last_write_tick;
    uint32_t last_error;
    uint16_t ring_used;
} ecu_data_logger_diag_t;

void ecu_data_logger_init(void);
TaskHandle_t ecu_data_logger_task_start(void *app_data);
void ecu_data_logger_can_rx_isr(uint32_t id,
                                bool is_standard,
                                bool is_remote,
                                uint8_t dlc,
                                const uint8_t data[8],
                                bool known_ams,
                                bool known_cm200,
                                bool parsed);
void ecu_data_logger_get_diag(ecu_data_logger_diag_t *out);
bool ecu_data_logger_request_start(void);
bool ecu_data_logger_request_stop(void);
bool ecu_data_logger_request_flush(void);
bool ecu_data_logger_request_new_session(void);
bool ecu_data_logger_set_rate(uint16_t hz);
void ecu_data_logger_set_raw(bool enabled);
void ecu_data_logger_mark(uint32_t marker);

#endif
