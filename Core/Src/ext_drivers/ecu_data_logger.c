/*
 * ecu_data_logger.c
 *
 * Best-effort telemetry recorder. It is intentionally below all ECU control
 * tasks, owns no safety state, and drops logger records rather than blocking
 * an ISR or changing torque authority.
 */
#include "ext_drivers/ecu_data_logger.h"

#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "cmsis_os.h"
#include "ext_drivers/sdcard_service.h"
#include "fatfs.h"

#define LOGGER_RAW_RING_CAPACITY 512u
#define LOGGER_RAW_RING_MASK (LOGGER_RAW_RING_CAPACITY - 1u)
#define LOGGER_RAW_BATCH 64u
#define LOGGER_SYNC_PERIOD_MS 1000u
#define LOGGER_AUTOSTART_DELAY_MS 1500u
#define LOGGER_RETRY_PERIOD_MS 5000u
#define LOGGER_TASK_PERIOD_MS 20u
#define LOGGER_MAX_CONSECUTIVE_ERROR_CYCLES 3u
#define LOGGER_FILE_SCHEMA 2u
#define LOGGER_RAW_SCHEMA 1u

_Static_assert((LOGGER_RAW_RING_CAPACITY & LOGGER_RAW_RING_MASK) == 0u,
               "raw logger ring capacity must be a power of two");

typedef struct __attribute__((packed))
{
    uint32_t timestamp_ms;
    uint32_t sequence;
    uint32_t id;
    uint8_t dlc;
    uint8_t flags;
    uint8_t data[8];
    uint16_t reserved;
} logger_raw_record_t;

_Static_assert(sizeof(logger_raw_record_t) == 24u,
               "raw CAN record format changed");

typedef enum
{
    LOGGER_REQUEST_NONE = 0,
    LOGGER_REQUEST_START,
    LOGGER_REQUEST_STOP,
    LOGGER_REQUEST_FLUSH,
    LOGGER_REQUEST_NEW
} logger_request_t;

typedef struct
{
    volatile uint16_t head;
    volatile uint16_t tail;
    logger_raw_record_t records[LOGGER_RAW_RING_CAPACITY];
    volatile uint32_t sequence;
    volatile uint32_t dropped;
    volatile uint32_t high_water;
} logger_raw_ring_t;

typedef struct
{
    ecu_data_logger_diag_t diag;
    logger_raw_ring_t ring;
    volatile logger_request_t request;
    volatile bool capture_active;
    volatile bool capture_raw_enabled;
    volatile bool mark_pending;
    volatile uint32_t mark_value;
    FIL decoded_file;
    FIL raw_file;
    FIL event_file;
    FIL meta_file;
    bool decoded_open;
    bool raw_open;
    bool event_open;
    bool meta_open;
    char decoded_name[13];
    char raw_name[13];
    char event_name[13];
    char meta_name[13];
    uint32_t last_decoded_tick;
    uint32_t last_retry_tick;
    uint32_t last_reported_drop;
    uint32_t last_reported_ams_snapshot_gap;
    uint32_t last_reported_ams_incomplete;
    uint16_t last_reported_ams_source_superseded;
    uint16_t last_reported_ams_source_deadline;
} logger_state_t;

static logger_state_t g_logger;
static StaticTask_t logger_task_tcb;
static StackType_t logger_task_stack[ECU_STACK_LOGGER_WORDS];
static TaskHandle_t logger_task_handle;

static void flush_raw_locked(void);
static bool set_request(logger_request_t request);
static bool requeue_request_if_none(logger_request_t request);

static void saturating_increment_u32(uint32_t *value)
{
    if((value != NULL) && (*value != UINT32_MAX))
    {
        (*value)++;
    }
}

static const char decoded_header[] =
    "ms,row,throttle_pct,brake_pct,rtd,hard_fault,soft_fault,startup_fault,"
    "fw_ok,mtr_en,mtr_on,brakelight,cool_press_milli,cool_flow_milli,"
    "cool_tin_mC,cool_tout_mC,cool_valid,cool_fault_flags,"
    "cool_pump_cmd_0p1pct,cool_pump_S_0p1pct,cool_pump_gate_0p1pct,"
    "cool_pump_flags,can_rx_ok,can_rx_ignored,"
    "can_rx_malformed,can_rx_remote,can_overrun,can_error,torque_target_0p1Nm,"
    "torque_command_0p1Nm,clamp_reason,clamp_valid,battery_authority,"
    "residual_fault,residual_violations,ams_allow,ams_stale,ams_bms_ok,"
    "ams_inhibit,ams_S,ams_E,ams_T,ams_H,ams_seq,ams_status_flags,"
    "ams_fault_flags,ams_pack_0p1V,ams_current_0p1A,ams_min_cell_mV,"
    "ams_max_cell_mV,ams_min_temp_0p1C,ams_max_temp_0p1C,ams_avg_temp_0p1C,"
    "ams_current_source,ams_current_quality,ams_current_age_ms,"
    "ams_logger_proto,ams_logger_seq,ams_logger_snapshot_seq,"
    "ams_logger_phase,ams_logger_phase_count,ams_logger_meas_seq,"
    "ams_logger_rx,ams_logger_age_ms,ams_detail_complete,ams_snapshot_gaps,"
    "ams_incomplete_snapshots,ams_phase_gaps,ams_cell_fragment_gaps,"
    "ams_temp_fragment_gaps,ams_duplicate_fragments,ams_out_of_order,"
    "ams_source_protected_deadline,ams_source_detail_superseded,"
    "ams_source_detail_recovery_drop,ams_source_protected_superseded,"
    "ams_source_tx_flags,apm_i1_0p01A,apm_i2_0p01A,"
    "apm_v1_0p1V,apm_v2_0p1V,apm_flags,apm_stage,apm_reason,"
    "power_valid,power_stale,dcl_0p1A,ccl_0p1A,dpl_W,cpl_W,cm200_ready,"
    "cm200_fault,cm200_speed_rpm,cm200_dc_0p1V,cm200_dc_0p1A,"
    "cm200_cmd_0p1Nm,cm200_feedback_0p1Nm,cm200_moduleA_0p1C,"
    "cm200_moduleB_0p1C,cm200_moduleC_0p1C,cm200_gate_0p1C,"
    "cm200_motor_0p1C,cm200_state,cm200_post_faults,cm200_run_faults\r\n";

static void compiler_barrier(void)
{
#if defined(__arm__) || defined(__thumb__)
    __DMB();
#else
    __asm__ volatile("" ::: "memory");
#endif
}

static uint16_t ring_used(void)
{
    return (uint16_t)((g_logger.ring.head - g_logger.ring.tail) &
                      LOGGER_RAW_RING_MASK);
}

static int32_t float_to_milli(float value)
{
    if(!isfinite(value)) return INT32_MIN;
    if(value >= 2147483.0f) return INT32_MAX;
    if(value <= -2147483.0f) return INT32_MIN;
    return (int32_t)(value * 1000.0f);
}

static int32_t float_to_deci(float value)
{
    if(!isfinite(value)) return INT32_MIN;
    if(value >= 214748364.0f) return INT32_MAX;
    if(value <= -214748364.0f) return INT32_MIN;
    return (int32_t)(value * 10.0f);
}

static bool write_bytes(FIL *file, const void *data, UINT len)
{
    UINT written = 0u;
    FRESULT r = f_write(file, data, len, &written);
    if((r != FR_OK) || (written != len))
    {
        saturating_increment_u32(&g_logger.diag.write_errors);
        g_logger.diag.last_error = (r == FR_OK) ? FR_DISK_ERR : (uint32_t)r;
        return false;
    }
    g_logger.diag.last_write_tick = HAL_GetTick();
    return true;
}

static bool write_text(FIL *file, const char *text)
{
    return (text != NULL) && write_bytes(file, text, (UINT)strlen(text));
}

static bool append_field(char *line, size_t size, size_t *used,
                         const char *fmt, ...)
{
    if((line == NULL) || (used == NULL) || (*used >= size)) return false;
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(&line[*used], size - *used, fmt, args);
    va_end(args);
    if((n < 0) || ((size_t)n >= (size - *used))) return false;
    *used += (size_t)n;
    return true;
}

static void make_names(uint16_t index)
{
    (void)snprintf(g_logger.decoded_name, sizeof(g_logger.decoded_name),
                   "ECU%03u.CSV", (unsigned)index);
    (void)snprintf(g_logger.raw_name, sizeof(g_logger.raw_name),
                   "CAN%03u.BIN", (unsigned)index);
    (void)snprintf(g_logger.event_name, sizeof(g_logger.event_name),
                   "EVT%03u.CSV", (unsigned)index);
    (void)snprintf(g_logger.meta_name, sizeof(g_logger.meta_name),
                   "META%03u.TXT", (unsigned)index);
}

static bool choose_session(void)
{
    FILINFO info;
    for(uint16_t i = 0u; i < 1000u; i++)
    {
        bool occupied = false;
        make_names(i);
        const char *names[] = {
            g_logger.decoded_name,
            g_logger.raw_name,
            g_logger.event_name,
            g_logger.meta_name
        };

        /* A power cut can leave any prefix of the four session files. Treat
         * the index as occupied if ANY file exists; probing META alone can
         * otherwise lock autostart onto an orphaned index forever. */
        for(size_t n = 0u; n < (sizeof(names) / sizeof(names[0])); n++)
        {
            FRESULT r = f_stat(names[n], &info);
            if(r == FR_OK)
            {
                occupied = true;
                break;
            }
            if(r != FR_NO_FILE)
            {
                g_logger.diag.last_error = (uint32_t)r;
                return false;
            }
        }

        if(!occupied)
        {
            g_logger.diag.session_index = i;
            return true;
        }
    }
    g_logger.diag.last_error = FR_EXIST;
    return false;
}

static void close_files_locked(void)
{
    if(g_logger.decoded_open)
    {
        (void)f_sync(&g_logger.decoded_file);
        (void)f_close(&g_logger.decoded_file);
        g_logger.decoded_open = false;
    }
    if(g_logger.raw_open)
    {
        (void)f_sync(&g_logger.raw_file);
        (void)f_close(&g_logger.raw_file);
        g_logger.raw_open = false;
    }
    if(g_logger.event_open)
    {
        (void)f_sync(&g_logger.event_file);
        (void)f_close(&g_logger.event_file);
        g_logger.event_open = false;
    }
    if(g_logger.meta_open)
    {
        (void)f_sync(&g_logger.meta_file);
        (void)f_close(&g_logger.meta_file);
        g_logger.meta_open = false;
    }
    g_logger.diag.files_open = false;
}

static bool logger_sync_locked(void)
{
    if(!g_logger.diag.files_open) return false;
    FRESULT r1 = f_sync(&g_logger.decoded_file);
    FRESULT r2 = f_sync(&g_logger.raw_file);
    FRESULT r3 = f_sync(&g_logger.event_file);
    if((r1 != FR_OK) || (r2 != FR_OK) || (r3 != FR_OK))
    {
        saturating_increment_u32(&g_logger.diag.write_errors);
        g_logger.diag.last_error = (uint32_t)((r1 != FR_OK) ? r1 :
                                  ((r2 != FR_OK) ? r2 : r3));
        return false;
    }
    saturating_increment_u32(&g_logger.diag.sync_count);
    g_logger.diag.last_sync_tick = HAL_GetTick();
    return true;
}

static void event_locked(const char *event, uint32_t value, const char *detail)
{
    char line[160];
    int n;
    if(!g_logger.diag.files_open) return;
    n = snprintf(line, sizeof(line), "%lu,%s,%lu,%s\r\n",
                 (unsigned long)HAL_GetTick(), event,
                 (unsigned long)value, (detail != NULL) ? detail : "");
    if((n > 0) && ((size_t)n < sizeof(line)) &&
       write_bytes(&g_logger.event_file, line, (UINT)n))
    {
        g_logger.diag.event_rows++;
    }
}

static bool open_session(void)
{
    uint8_t raw_header[32] = {0};
    char meta[768];
    int n;
    FRESULT r;

    if(!sdcard_service_lock(1500u))
    {
        g_logger.diag.last_error = FR_TIMEOUT;
        return false;
    }

    r = sdcard_service_mount();
    if((r != FR_OK) || !choose_session())
    {
        g_logger.diag.last_error = (r != FR_OK) ? (uint32_t)r :
                                   g_logger.diag.last_error;
        sdcard_service_unlock();
        return false;
    }

    r = f_open(&g_logger.decoded_file, g_logger.decoded_name,
               FA_CREATE_NEW | FA_WRITE);
    if(r == FR_OK)
    {
        g_logger.decoded_open = true;
        r = f_open(&g_logger.raw_file, g_logger.raw_name,
                   FA_CREATE_NEW | FA_WRITE);
    }
    if(r == FR_OK)
    {
        g_logger.raw_open = true;
        r = f_open(&g_logger.event_file, g_logger.event_name,
                   FA_CREATE_NEW | FA_WRITE);
    }
    if(r == FR_OK)
    {
        g_logger.event_open = true;
        r = f_open(&g_logger.meta_file, g_logger.meta_name,
                   FA_CREATE_NEW | FA_WRITE);
    }
    if(r == FR_OK) g_logger.meta_open = true;
    if(r != FR_OK)
    {
        g_logger.diag.last_error = (uint32_t)r;
        close_files_locked();
        sdcard_service_unlock();
        return false;
    }
    g_logger.diag.files_open = true;

    memcpy(raw_header, "DERCAN1", 7u);
    raw_header[8] = LOGGER_RAW_SCHEMA;
    raw_header[10] = (uint8_t)(sizeof(logger_raw_record_t) & 0xFFu);
    raw_header[11] = (uint8_t)(sizeof(logger_raw_record_t) >> 8u);
    raw_header[12] = (uint8_t)VER_MAJOR;
    raw_header[13] = (uint8_t)VER_MINOR;
    raw_header[14] = (uint8_t)VER_BUG;

    n = snprintf(meta, sizeof(meta),
        "DER26 ECU SD/CAN logger\r\n"
        "schema=%u\r\nfirmware=%u.%u.%u\r\nprofile=%s\r\n"
        "source_revision=%s\r\nconfig_fingerprint=0x%08lX\r\n"
        "can_contract=%s\r\nlogger_schema=%s\r\nbuild_date=%s\r\nbuild_time=%s\r\n"
        "session=%u\r\ndecoded=%s\r\nraw=%s\r\nevents=%s\r\n"
        "raw_record_size=%u\r\nraw_endian=little\r\n"
        "raw_flags=bit0_standard,bit1_remote,bit2_AMS,bit3_CM200,bit4_parsed\r\n"
        "decoded_units=encoded_in_column_names\r\n",
        LOGGER_FILE_SCHEMA, VER_MAJOR, VER_MINOR, VER_BUG,
        ECU_BUILD_PROFILE_NAME,
        ECU_BUILD_SOURCE_REVISION,
        (unsigned long)ECU_BUILD_CONFIG_FINGERPRINT,
        DER26_CAN_CONTRACT_NAME, ECU_CAN_LOGGER_SCHEMA_REVISION,
        __DATE__, __TIME__,
        (unsigned)g_logger.diag.session_index,
        g_logger.decoded_name, g_logger.raw_name, g_logger.event_name,
        (unsigned)sizeof(logger_raw_record_t));

    if((n <= 0) || ((size_t)n >= sizeof(meta)) ||
       !write_text(&g_logger.decoded_file, decoded_header) ||
       !write_bytes(&g_logger.raw_file, raw_header, sizeof(raw_header)) ||
       !write_text(&g_logger.event_file, "ms,event,value,detail\r\n") ||
       !write_bytes(&g_logger.meta_file, meta, (UINT)n))
    {
        close_files_locked();
        sdcard_service_unlock();
        return false;
    }

    (void)f_sync(&g_logger.meta_file);
    g_logger.diag.last_error = FR_OK;
    g_logger.diag.active = true;
    g_logger.capture_active = true;
    g_logger.diag.start_tick = HAL_GetTick();
    g_logger.diag.last_sync_tick = HAL_GetTick();
    g_logger.last_decoded_tick = 0u;
    event_locked("START", g_logger.diag.session_index, "logger_active");
    (void)logger_sync_locked();
    sdcard_service_unlock();
    return true;
}

static bool stop_session(void)
{
    /* Stop the ISR producer before draining and closing the files. */
    g_logger.capture_active = false;
    g_logger.diag.active = false;

    if(!sdcard_service_lock(3000u))
    {
        saturating_increment_u32(&g_logger.diag.write_errors);
        g_logger.diag.last_error = FR_TIMEOUT;
        return false;
    }

    flush_raw_locked();
    event_locked("STOP", g_logger.diag.session_index, "logger_stopped");
    (void)logger_sync_locked();
    close_files_locked();
    sdcard_service_unlock();
    g_logger.ring.tail = g_logger.ring.head;
    return true;
}

static void flush_raw_locked(void)
{
    logger_raw_record_t batch[LOGGER_RAW_BATCH];
    uint16_t count = 0u;

    while((count < LOGGER_RAW_BATCH) &&
          (g_logger.ring.tail != g_logger.ring.head))
    {
        compiler_barrier();
        batch[count] = g_logger.ring.records[g_logger.ring.tail];
        g_logger.ring.tail = (uint16_t)((g_logger.ring.tail + 1u) &
                                        LOGGER_RAW_RING_MASK);
        count++;
    }
    if(count != 0u)
    {
        if(write_bytes(&g_logger.raw_file, batch,
                       (UINT)(count * sizeof(batch[0]))))
        {
            g_logger.diag.raw_records += count;
        }
    }
}

typedef struct
{
    uint32_t now;
    int throttle, brake;
    uint8_t rtd;
    bool hard_fault, soft_fault, startup_fault;
    bool fw_ok, mtr_en, mtr_on, brakelight;
    float cool_pressure, cool_flow, cool_tin, cool_tout;
    bool cool_valid;
    uint16_t cool_fault_flags;
    float cool_pump_cmd, cool_pump_s, cool_pump_gate;
    uint8_t cool_pump_flags;
    uint32_t can_rx_ok, can_ignored, can_malformed, can_remote;
    uint32_t can_overrun, can_error;
    int16_t target_torque, command_torque;
    uint8_t clamp_reason, battery_authority;
    bool clamp_valid, residual_fault;
    uint32_t residual_violations;

    /* Only fields serialized to ECU###.CSV are copied from the AMS/CM200
     * objects.  Do not memcpy the complete protocol state while interrupts are
     * masked: those objects contain large cell/temp and protocol histories and
     * are not needed by the logger. */
    bool ams_allow, ams_stale, ams_bms_ok, ams_bms_inhibited;
    bool ams_status_valid, ams_electrical_valid, ams_thermal_valid;
    bool ams_health_valid;
    uint8_t ams_sequence, ams_status_flags, ams_fault_flags;
    uint16_t ams_pack_0p1v;
    int16_t ams_current_0p1a;
    uint16_t ams_min_cell_mv, ams_max_cell_mv;
    int16_t ams_min_temp_0p1c, ams_max_temp_0p1c, ams_avg_temp_0p1c;
    uint8_t ams_current_source, ams_current_quality;
    uint16_t ams_current_age_ms;
    uint8_t ams_logger_proto, ams_logger_seq, ams_logger_snapshot_seq;
    uint8_t ams_logger_phase, ams_logger_phase_count;
    uint32_t ams_logger_meas_seq, ams_logger_rx, ams_logger_last_rx_tick;
    bool ams_detail_complete;
    uint32_t ams_snapshot_gaps, ams_incomplete_snapshots, ams_phase_gaps;
    uint32_t ams_cell_fragment_gaps, ams_temp_fragment_gaps;
    uint32_t ams_duplicate_fragments, ams_out_of_order;
    uint16_t ams_source_protected_deadline, ams_source_detail_superseded;
    uint16_t ams_source_detail_recovery_drop;
    uint8_t ams_source_protected_superseded, ams_source_tx_flags;
    int16_t apm_i1_0p01a, apm_i2_0p01a;
    uint16_t apm_v1_0p1v, apm_v2_0p1v;
    uint8_t apm_flags, apm_stage, apm_reason;
    bool power_valid, power_stale;
    int32_t dcl_0p1a, ccl_0p1a, dpl_w, cpl_w;

    bool cm200_ready, cm200_fault;
    int16_t cm200_speed_rpm, cm200_dc_0p1v, cm200_dc_0p1a;
    int16_t cm200_cmd_0p1nm, cm200_feedback_0p1nm;
    int16_t cm200_module_a_0p1c, cm200_module_b_0p1c;
    int16_t cm200_module_c_0p1c, cm200_gate_0p1c, cm200_motor_0p1c;
    uint8_t cm200_state;
    uint32_t cm200_post_faults, cm200_run_faults;
} decoded_snapshot_t;

/* Keep the interrupt-masked logger snapshot small enough that future protocol
 * growth cannot silently reintroduce the old whole-object critical copy. */
_Static_assert(sizeof(decoded_snapshot_t) <= 256u,
               "decoded logger snapshot critical copy exceeded 256 bytes");

static void capture_snapshot(const app_data_t *app, decoded_snapshot_t *s)
{
    /* The snapshot is logger-task-owned. Clear it before masking interrupts so
     * the critical section contains only the bounded coherent field copy. */
    memset(s, 0, sizeof(*s));
    taskENTER_CRITICAL();
    s->now = HAL_GetTick();
    s->throttle = app->throttle;
    s->brake = app->brake;
    s->rtd = (uint8_t)app->rtd_mode;
    s->hard_fault = app->hard_fault;
    s->soft_fault = app->soft_fault;
    s->startup_fault = app->startup_fault;
    s->fw_ok = app->fw_state;
    s->mtr_en = app->cascadia_en;
    s->mtr_on = app->cascadia_on;
    s->brakelight = app->brakelight;
    s->cool_pressure = app->coolant_pressure;
    s->cool_flow = app->coolant_flow;
    s->cool_tin = app->coolant_temp_in;
    s->cool_tout = app->coolant_temp_out;
    s->cool_valid = app->coolant_telemetry_valid;
    s->cool_fault_flags = app->coolant_fault_flags;
    s->cool_pump_cmd = app->coolant_pump_command_percent;
    s->cool_pump_s = app->coolant_pump_s_duty_percent;
    s->cool_pump_gate = app->coolant_pump_gate_duty_percent;
    s->cool_pump_flags = app->coolant_pump_flags;
    s->can_rx_ok = app->board.canbus.rx_accepted_count;
    s->can_ignored = app->board.canbus.rx_ignored_count;
    s->can_malformed = app->board.canbus.rx_malformed_count;
    s->can_remote = app->board.canbus.rx_remote_count;
    s->can_overrun = app->can_rx_overrun_count;
    s->can_error = app->can_error_code;
    s->target_torque = app->cm200_target_torque_0p1nm;
    s->command_torque = app->cm200_command_torque_0p1nm;
    s->clamp_reason = app->torque_clamp_reason;
    s->clamp_valid = app->torque_clamp_output_valid;
    s->battery_authority = app->battery_authority_state;
    s->residual_fault = app->current_model_residual_fault;
    s->residual_violations = app->current_residual_violation_count;

    const ams_t *ams = &app->board.ams;
    s->ams_allow = ams_allows_torque(ams);
    s->ams_stale = ams->stale;
    s->ams_bms_ok = ams->bms_ok;
    s->ams_bms_inhibited = ams->bms_inhibited;
    s->ams_status_valid = ams->compact_status_valid;
    s->ams_electrical_valid = ams->compact_electrical_valid;
    s->ams_thermal_valid = ams->compact_thermal_valid;
    s->ams_health_valid = ams->compact_health_valid;
    s->ams_sequence = ams->compact_sequence;
    s->ams_status_flags = ams->compact_status_flags;
    s->ams_fault_flags = ams->compact_fault_flags;
    s->ams_pack_0p1v = ams->pack_voltage_0p1v;
    s->ams_current_0p1a = ams->pack_current_0p1a;
    s->ams_min_cell_mv = ams->min_cell_mv;
    s->ams_max_cell_mv = ams->max_cell_mv;
    s->ams_min_temp_0p1c = ams->min_temp_0p1c;
    s->ams_max_temp_0p1c = ams->max_temp_0p1c;
    s->ams_avg_temp_0p1c = ams->avg_temp_0p1c;
    s->ams_current_source = ams->current_source;
    s->ams_current_quality = ams->current_quality;
    s->ams_current_age_ms = ams->current_sample_age_ms;
    s->ams_logger_proto = ams->logger_protocol_version;
    s->ams_logger_seq = ams->logger_sequence;
    s->ams_logger_snapshot_seq = ams->logger_snapshot_sequence;
    s->ams_logger_phase = ams->logger_phase;
    s->ams_logger_phase_count = ams->logger_phase_count;
    s->ams_logger_meas_seq = ams->logger_measurement_sequence;
    s->ams_logger_rx = ams->logger_rx_count;
    s->ams_logger_last_rx_tick = ams->logger_last_rx_tick;
    s->ams_detail_complete = ams->logger_snapshot_complete;
    s->ams_snapshot_gaps = ams->logger_snapshot_gap_count;
    s->ams_incomplete_snapshots = ams->logger_incomplete_snapshot_count;
    s->ams_phase_gaps = ams->logger_phase_gap_count;
    s->ams_cell_fragment_gaps = ams->logger_cell_fragment_gap_count;
    s->ams_temp_fragment_gaps = ams->logger_temp_fragment_gap_count;
    s->ams_duplicate_fragments = ams->logger_duplicate_fragment_count;
    s->ams_out_of_order = ams->logger_out_of_order_count;
    s->ams_source_protected_deadline = ams->tx_sched_protected_deadline_miss;
    s->ams_source_detail_superseded = ams->tx_sched_detail_superseded;
    s->ams_source_detail_recovery_drop = ams->tx_sched_detail_recovery_discard;
    s->ams_source_protected_superseded = ams->tx_sched_protected_superseded;
    s->ams_source_tx_flags = ams->tx_sched_flags;
    s->apm_i1_0p01a = ams->apm_current1_0p01a;
    s->apm_i2_0p01a = ams->apm_current2_0p01a;
    s->apm_v1_0p1v = ams->apm_voltage1_0p1v;
    s->apm_v2_0p1v = ams->apm_voltage2_0p1v;
    s->apm_flags = ams->apm_flags;
    s->apm_stage = ams->apm_stage;
    s->apm_reason = ams->apm_reason;
    s->power_valid = ams->power_authority_valid;
    s->power_stale = ams->power_authority_stale;
    if(ams->power_authority_valid)
    {
        s->dcl_0p1a = float_to_deci(
            ams->power_authority.discharge.current_limit_a);
        s->ccl_0p1a = float_to_deci(
            ams->power_authority.charge_regen.current_limit_a);
        s->dpl_w = (int32_t)ams->power_authority.discharge.power_limit_w;
        s->cpl_w = (int32_t)ams->power_authority.charge_regen.power_limit_w;
    }

    const cm200_t *cm200 = &app->board.cm200;
    s->cm200_ready = app->cm200_ready;
    s->cm200_fault = app->cm200_fault;
    s->cm200_speed_rpm = cm200->motor_speed_rpm;
    s->cm200_dc_0p1v = cm200->dc_bus_voltage_0p1v;
    s->cm200_dc_0p1a = cm200->dc_bus_current_0p1a;
    s->cm200_cmd_0p1nm = cm200->commanded_torque_0p1nm;
    s->cm200_feedback_0p1nm = cm200->torque_feedback_0p1nm;
    s->cm200_module_a_0p1c = cm200->module_a_temp_0p1c;
    s->cm200_module_b_0p1c = cm200->module_b_temp_0p1c;
    s->cm200_module_c_0p1c = cm200->module_c_temp_0p1c;
    s->cm200_gate_0p1c = cm200->gate_driver_temp_0p1c;
    s->cm200_motor_0p1c = cm200->motor_temp_0p1c;
    s->cm200_state = cm200->inverter_state;
    s->cm200_post_faults = cm200->post_faults;
    s->cm200_run_faults = cm200->run_faults;
    taskEXIT_CRITICAL();
}

static void write_decoded_locked(const app_data_t *app)
{
    decoded_snapshot_t s;
    char line[2048];
    size_t n = 0u;
    uint32_t logger_age;

    capture_snapshot(app, &s);
    logger_age = (s.ams_logger_last_rx_tick != 0u) ?
                 (uint32_t)(s.now - s.ams_logger_last_rx_tick) : UINT32_MAX;

#define FMT(...) do { if(!append_field(line, sizeof(line), &n, __VA_ARGS__)) return; } while(0)
    FMT("%lu,%lu", (unsigned long)s.now,
        (unsigned long)g_logger.diag.decoded_rows);
#define U(v) FMT(",%lu", (unsigned long)(v))
#define I(v) FMT(",%ld", (long)(v))
    I(s.throttle); I(s.brake); U(s.rtd); U(s.hard_fault); U(s.soft_fault);
    U(s.startup_fault); U(s.fw_ok); U(s.mtr_en); U(s.mtr_on); U(s.brakelight);
    I(float_to_milli(s.cool_pressure)); I(float_to_milli(s.cool_flow));
    I(float_to_milli(s.cool_tin)); I(float_to_milli(s.cool_tout)); U(s.cool_valid);
    U(s.cool_fault_flags); I(float_to_deci(s.cool_pump_cmd));
    I(float_to_deci(s.cool_pump_s)); I(float_to_deci(s.cool_pump_gate));
    U(s.cool_pump_flags);
    U(s.can_rx_ok); U(s.can_ignored); U(s.can_malformed); U(s.can_remote);
    U(s.can_overrun); U(s.can_error); I(s.target_torque); I(s.command_torque);
    U(s.clamp_reason); U(s.clamp_valid); U(s.battery_authority);
    U(s.residual_fault); U(s.residual_violations);
    U(s.ams_allow); U(s.ams_stale); U(s.ams_bms_ok);
    U(s.ams_bms_inhibited); U(s.ams_status_valid);
    U(s.ams_electrical_valid); U(s.ams_thermal_valid);
    U(s.ams_health_valid); U(s.ams_sequence);
    U(s.ams_status_flags); U(s.ams_fault_flags);
    U(s.ams_pack_0p1v); I(s.ams_current_0p1a);
    U(s.ams_min_cell_mv); U(s.ams_max_cell_mv); I(s.ams_min_temp_0p1c);
    I(s.ams_max_temp_0p1c); I(s.ams_avg_temp_0p1c);
    U(s.ams_current_source); U(s.ams_current_quality); U(s.ams_current_age_ms);
    U(s.ams_logger_proto); U(s.ams_logger_seq);
    U(s.ams_logger_snapshot_seq); U(s.ams_logger_phase);
    U(s.ams_logger_phase_count); U(s.ams_logger_meas_seq);
    U(s.ams_logger_rx); U(logger_age);
    U(s.ams_detail_complete); U(s.ams_snapshot_gaps);
    U(s.ams_incomplete_snapshots); U(s.ams_phase_gaps);
    U(s.ams_cell_fragment_gaps); U(s.ams_temp_fragment_gaps);
    U(s.ams_duplicate_fragments); U(s.ams_out_of_order);
    U(s.ams_source_protected_deadline); U(s.ams_source_detail_superseded);
    U(s.ams_source_detail_recovery_drop); U(s.ams_source_protected_superseded);
    U(s.ams_source_tx_flags);
    I(s.apm_i1_0p01a); I(s.apm_i2_0p01a);
    U(s.apm_v1_0p1v); U(s.apm_v2_0p1v);
    U(s.apm_flags); U(s.apm_stage); U(s.apm_reason);
    U(s.power_valid); U(s.power_stale);
    I(s.dcl_0p1a); I(s.ccl_0p1a); I(s.dpl_w); I(s.cpl_w);
    U(s.cm200_ready); U(s.cm200_fault);
    I(s.cm200_speed_rpm); I(s.cm200_dc_0p1v);
    I(s.cm200_dc_0p1a); I(s.cm200_cmd_0p1nm);
    I(s.cm200_feedback_0p1nm); I(s.cm200_module_a_0p1c);
    I(s.cm200_module_b_0p1c); I(s.cm200_module_c_0p1c);
    I(s.cm200_gate_0p1c); I(s.cm200_motor_0p1c);
    U(s.cm200_state); U(s.cm200_post_faults); U(s.cm200_run_faults);
    FMT("\r\n");
#undef I
#undef U
#undef FMT

    if(write_bytes(&g_logger.decoded_file, line, (UINT)n))
    {
        g_logger.diag.decoded_rows++;
    }
}

static void handle_request(const app_data_t *app)
{
    logger_request_t request;
    taskENTER_CRITICAL();
    request = g_logger.request;
    g_logger.request = LOGGER_REQUEST_NONE;
    taskEXIT_CRITICAL();

    switch(request)
    {
    case LOGGER_REQUEST_START:
        g_logger.diag.auto_start = false;
        if(!g_logger.diag.active) (void)open_session();
        break;
    case LOGGER_REQUEST_STOP:
        g_logger.diag.auto_start = false;
        if((g_logger.diag.active || g_logger.diag.files_open) &&
           !stop_session())
        {
            /* A filesystem operation may briefly own the recursive mutex.
             * Retry instead of leaving open files with no close request. */
            (void)requeue_request_if_none(LOGGER_REQUEST_STOP);
        }
        break;
    case LOGGER_REQUEST_FLUSH:
        if(g_logger.diag.files_open && sdcard_service_lock(1500u))
        {
            flush_raw_locked();
            (void)logger_sync_locked();
            sdcard_service_unlock();
        }
        break;
    case LOGGER_REQUEST_NEW:
        g_logger.diag.auto_start = false;
        if(g_logger.diag.active || g_logger.diag.files_open)
        {
            if(!stop_session())
            {
                (void)requeue_request_if_none(LOGGER_REQUEST_NEW);
                break;
            }
        }
        (void)open_session();
        break;
    case LOGGER_REQUEST_NONE:
    default:
        (void)app;
        break;
    }
}

static void logger_task_fn(void *arg)
{
    app_data_t *app = (app_data_t *)arg;
    uint32_t boot = HAL_GetTick();

    for(;;)
    {
        uint32_t now = HAL_GetTick();
        handle_request(app);

        if(g_logger.diag.auto_start && !g_logger.diag.active &&
           ((uint32_t)(now - boot) >= LOGGER_AUTOSTART_DELAY_MS) &&
           ((uint32_t)(now - g_logger.last_retry_tick) >= LOGGER_RETRY_PERIOD_MS))
        {
            g_logger.last_retry_tick = now;
            (void)open_session();
        }

        uint32_t write_errors_before = g_logger.diag.write_errors;
        bool logger_cycle_attempted = false;
        if(g_logger.diag.active && g_logger.diag.files_open &&
           sdcard_service_lock(250u))
        {
            logger_cycle_attempted = true;
            flush_raw_locked();
            if((g_logger.last_decoded_tick == 0u) ||
               ((uint32_t)(now - g_logger.last_decoded_tick) >=
                (1000u / g_logger.diag.rate_hz)))
            {
                g_logger.last_decoded_tick = now;
                write_decoded_locked(app);
            }

            if(g_logger.mark_pending)
            {
                uint32_t marker;
                taskENTER_CRITICAL();
                marker = g_logger.mark_value;
                g_logger.mark_pending = false;
                taskEXIT_CRITICAL();
                event_locked("MARK", marker, "operator_marker");
            }

            if(g_logger.ring.dropped != g_logger.last_reported_drop)
            {
                g_logger.last_reported_drop = g_logger.ring.dropped;
                event_locked("RAW_DROP", g_logger.last_reported_drop,
                             "ISR_ring_overflow");
            }

            /* Detail-loss provenance is eventized as absolute saturating
             * counters. Post-run analysis can distinguish AMS source
             * supersession from ECU-observed fragment gaps and raw-ring loss. */
            if(app->board.ams.logger_snapshot_gap_count !=
               g_logger.last_reported_ams_snapshot_gap)
            {
                g_logger.last_reported_ams_snapshot_gap =
                    app->board.ams.logger_snapshot_gap_count;
                event_locked("AMS_SNAPSHOT_GAP",
                             g_logger.last_reported_ams_snapshot_gap,
                             "ECU_observed_sequence_discontinuity");
            }
            if(app->board.ams.logger_incomplete_snapshot_count !=
               g_logger.last_reported_ams_incomplete)
            {
                g_logger.last_reported_ams_incomplete =
                    app->board.ams.logger_incomplete_snapshot_count;
                event_locked("AMS_DETAIL_INCOMPLETE",
                             g_logger.last_reported_ams_incomplete,
                             "missing_phase_or_fragment");
            }
            if(app->board.ams.tx_sched_detail_superseded !=
               g_logger.last_reported_ams_source_superseded)
            {
                g_logger.last_reported_ams_source_superseded =
                    app->board.ams.tx_sched_detail_superseded;
                event_locked("AMS_SOURCE_SUPERSEDE",
                             g_logger.last_reported_ams_source_superseded,
                             "AMS_reported_detail_supersession");
            }
            if(app->board.ams.tx_sched_protected_deadline_miss !=
               g_logger.last_reported_ams_source_deadline)
            {
                g_logger.last_reported_ams_source_deadline =
                    app->board.ams.tx_sched_protected_deadline_miss;
                event_locked("AMS_PROTECTED_DEADLINE",
                             g_logger.last_reported_ams_source_deadline,
                             "AMS_reported_required_deadline_miss");
            }

            if((uint32_t)(now - g_logger.diag.last_sync_tick) >=
               LOGGER_SYNC_PERIOD_MS)
            {
                (void)logger_sync_locked();
            }
            sdcard_service_unlock();
        }

        if(logger_cycle_attempted)
        {
            if(g_logger.diag.write_errors != write_errors_before)
            {
                if(g_logger.diag.consecutive_error_cycles != UINT16_MAX)
                {
                    g_logger.diag.consecutive_error_cycles++;
                }
                if(g_logger.diag.consecutive_error_cycles >=
                   LOGGER_MAX_CONSECUTIVE_ERROR_CYCLES)
                {
                    /* A removed/dead card must not trigger filesystem work
                     * forever. Disable automatic restart and make one
                     * best-effort close attempt; logging stays non-safety. */
                    g_logger.diag.auto_start = false;
                    saturating_increment_u32(&g_logger.diag.auto_stop_count);
                    (void)stop_session();
                    g_logger.diag.consecutive_error_cycles = 0u;
                }
            }
            else
            {
                g_logger.diag.consecutive_error_cycles = 0u;
            }
        }

        g_logger.diag.raw_dropped = g_logger.ring.dropped;
        g_logger.diag.raw_high_water = g_logger.ring.high_water;
        g_logger.diag.ring_used = ring_used();
        osDelay(LOGGER_TASK_PERIOD_MS);
    }
}

void ecu_data_logger_init(void)
{
    memset(&g_logger, 0, sizeof(g_logger));
    g_logger.diag.enabled = (ECU_DATA_LOGGER_ENABLE != 0);
    g_logger.diag.raw_enabled = true;
    g_logger.capture_raw_enabled = true;
    g_logger.diag.auto_start = (ECU_DATA_LOGGER_AUTOSTART != 0);
    g_logger.diag.rate_hz = ECU_DATA_LOGGER_DEFAULT_HZ;
}

TaskHandle_t ecu_data_logger_task_start(void *app_data)
{
    if((app_data == NULL) || !g_logger.diag.enabled) return NULL;
    if(logger_task_handle == NULL)
    {
        logger_task_handle = xTaskCreateStatic(logger_task_fn, "SD logger",
            ECU_STACK_LOGGER_WORDS, app_data, LOGGER_PRIO,
            logger_task_stack, &logger_task_tcb);
    }
    return logger_task_handle;
}

void ecu_data_logger_can_rx_isr(uint32_t id,
                                bool is_standard,
                                bool is_remote,
                                uint8_t dlc,
                                const uint8_t data[8],
                                bool known_ams,
                                bool known_cm200,
                                bool parsed)
{
    if(!g_logger.capture_active || !g_logger.capture_raw_enabled ||
       (data == NULL))
        return;

    uint16_t head = g_logger.ring.head;
    uint16_t next = (uint16_t)((head + 1u) & LOGGER_RAW_RING_MASK);
    if(next == g_logger.ring.tail)
    {
        g_logger.ring.dropped++;
        return;
    }

    logger_raw_record_t *r = &g_logger.ring.records[head];
    r->timestamp_ms = HAL_GetTick();
    r->sequence = ++g_logger.ring.sequence;
    r->id = id;
    r->dlc = (dlc <= 8u) ? dlc : 8u;
    r->flags = (is_standard ? (1u << 0u) : 0u) |
               (is_remote ? (1u << 1u) : 0u) |
               (known_ams ? (1u << 2u) : 0u) |
               (known_cm200 ? (1u << 3u) : 0u) |
               (parsed ? (1u << 4u) : 0u);
    memcpy(r->data, data, 8u);
    r->reserved = 0u;
    compiler_barrier();
    g_logger.ring.head = next;
    uint32_t used = (uint32_t)((next - g_logger.ring.tail) &
                               LOGGER_RAW_RING_MASK);
    if(used > g_logger.ring.high_water) g_logger.ring.high_water = used;
}

void ecu_data_logger_get_diag(ecu_data_logger_diag_t *out)
{
    if(out == NULL) return;
    taskENTER_CRITICAL();
    *out = g_logger.diag;
    out->raw_dropped = g_logger.ring.dropped;
    out->raw_high_water = g_logger.ring.high_water;
    out->ring_used = ring_used();
    taskEXIT_CRITICAL();
}

static bool set_request(logger_request_t request)
{
    if(!g_logger.diag.enabled) return false;
    taskENTER_CRITICAL();
    g_logger.request = request;
    taskEXIT_CRITICAL();
    return true;
}

static bool requeue_request_if_none(logger_request_t request)
{
    bool queued = false;
    if(!g_logger.diag.enabled) return false;
    taskENTER_CRITICAL();
    if(g_logger.request == LOGGER_REQUEST_NONE)
    {
        g_logger.request = request;
        queued = true;
    }
    taskEXIT_CRITICAL();
    return queued;
}

bool ecu_data_logger_request_start(void) { return set_request(LOGGER_REQUEST_START); }
bool ecu_data_logger_request_stop(void) { return set_request(LOGGER_REQUEST_STOP); }
bool ecu_data_logger_request_flush(void) { return set_request(LOGGER_REQUEST_FLUSH); }
bool ecu_data_logger_request_new_session(void) { return set_request(LOGGER_REQUEST_NEW); }

bool ecu_data_logger_set_rate(uint16_t hz)
{
    if((hz < 1u) || (hz > 100u)) return false;
    taskENTER_CRITICAL();
    g_logger.diag.rate_hz = hz;
    taskEXIT_CRITICAL();
    return true;
}

void ecu_data_logger_set_raw(bool enabled)
{
    taskENTER_CRITICAL();
    g_logger.capture_raw_enabled = enabled;
    g_logger.diag.raw_enabled = enabled;
    if(!enabled) g_logger.ring.tail = g_logger.ring.head;
    taskEXIT_CRITICAL();
}

void ecu_data_logger_mark(uint32_t marker)
{
    taskENTER_CRITICAL();
    g_logger.mark_value = marker;
    g_logger.mark_pending = true;
    taskEXIT_CRITICAL();
}
