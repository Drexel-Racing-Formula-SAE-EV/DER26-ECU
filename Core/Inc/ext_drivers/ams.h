/*
 * ams.h
 *
 *  Created on: Mar 28, 2024
 *      Author: cole
 */

#ifndef __AMS_H_
#define __AMS_H_

#include <stdbool.h>
#include <stdint.h>

#include "ext_drivers/ams_power_consumer.h"

#define NSEGS 5
#define NFANS 10
#define NVOLTS 15
#define NTEMPS 24

/* Legacy paged AMS telemetry. Kept for bench compatibility. */
#define AMS_TELEM_CANBUS_ID 0x69u
#define AMS_ESTIMATOR_CANBUS_ID 0x421u
#define AMS_PACKET_COUNT 72u

/* Compact AMS->ECU frames from the hardened AMS firmware. */
#define AMS_ECU_STATUS_CANBUS_ID 0x680u
#define AMS_ECU_ELECTRICAL_CANBUS_ID 0x681u
#define AMS_ECU_THERMAL_CANBUS_ID 0x682u
#define AMS_ECU_HEALTH_CANBUS_ID 0x683u
#define AMS_ECU_CURRENT_DIAG_CANBUS_ID 0x68Bu
#define AMS_ECU_COMPACT_PROTOCOL_VERSION 1u

/* Authoritative SoP/SoH protocol v2 from AMS v0.3.3+. */
#ifndef AMS_POWER_AUTHORITY_REQUIRED_FOR_TORQUE
#define AMS_POWER_AUTHORITY_REQUIRED_FOR_TORQUE 1u
#endif

#define AMS_FRAME_DLC 8u
#define AMS_STALE_TIMEOUT_MS 500u

/* Physical-plausibility envelope for summaries used by the ECU torque gate.
 * These are data-integrity bounds, not the AMS's operational trip limits. */
#define AMS_CELL_VALID_MIN_MV       500u
#define AMS_CELL_VALID_MAX_MV      5000u
#define AMS_PACK_VALID_MAX_0P1V   10000u
/* 0x681 pack voltage is rounded to 0.1 V; allow source/cell-sum rounding. */
#define AMS_PACK_CELL_SUM_TOLERANCE_MV 200u
#define AMS_CURRENT_VALID_MIN_0P1A (-10000)
#define AMS_CURRENT_VALID_MAX_0P1A  10000
#define AMS_TEMP_VALID_MIN_0P1C     (-400)
#define AMS_TEMP_VALID_MAX_0P1C      1500
#define AMS_TOTAL_CELL_COUNT           75u
#define AMS_TOTAL_TEMP_COUNT          120u

_Static_assert(AMS_TOTAL_CELL_COUNT == (NSEGS * NVOLTS),
               "AMS cell-count contract must match segment layout");
_Static_assert(AMS_TOTAL_TEMP_COUNT == (NSEGS * NTEMPS),
               "AMS temperature-count contract must match segment layout");
_Static_assert(AMS_PACKET_COUNT ==
               (3u + (NSEGS * 5u) + (NSEGS * 8u) + 4u),
               "Legacy AMS packet count must cover status, cells, temps, fans");

#define AMS_THERMAL_OVERTEMP_FAULT_BIT       4u
#define AMS_THERMAL_SEVERE_OVERTEMP_BIT      5u
#define AMS_THERMAL_INVALID_OR_READ_FAULT_BIT 7u
#define AMS_THERMAL_TORQUE_BLOCK_MASK \
    ((uint8_t)((1u << AMS_THERMAL_OVERTEMP_FAULT_BIT) | \
               (1u << AMS_THERMAL_SEVERE_OVERTEMP_BIT) | \
               (1u << AMS_THERMAL_INVALID_OR_READ_FAULT_BIT)))

typedef struct
{
    uint16_t header;
    uint16_t *d0;
    uint16_t *d1;
    uint16_t *d2;
} ams_data_packet_t;

typedef struct
{
    uint16_t volts[NVOLTS];
    uint16_t temps[NTEMPS];
} segment_t;

typedef struct
{
    uint16_t words[4];

    uint32_t last_rx_tick;
    uint32_t rx_count;
    bool valid;
} ams_estimator_status_t;

typedef struct
{
    uint16_t state;
    uint16_t air_state;
    uint16_t imd_ok;
    uint16_t imd_status;
    uint16_t imd_duty;
    uint16_t current;
    uint16_t max_temp;
    uint16_t min_volt;
    uint16_t max_volt;
    segment_t segs[NSEGS];
    uint16_t fans[NFANS];
    ams_estimator_status_t estimator;

    /* Compact 0x680 status frame. */
    uint8_t compact_protocol_version;
    uint8_t compact_sequence;
    uint8_t compact_state;
    uint8_t compact_status_flags;
    uint8_t compact_fault_flags;
    uint8_t voltage_fault_reason;
    uint8_t temp_fault_reason;
    uint8_t current_fault_reason;
    bool compact_status_valid;
    bool compact_protocol_valid;
    bool bms_ok;
    bool bms_inhibited;
    bool ams_hard_fault;
    bool ams_soft_fault;
    bool voltage_valid;
    bool current_valid;
    bool temp_valid;
    bool ams_can_fault;
    bool voltage_fault;
    bool temp_fault;
    bool current_fault;
    bool charger_fault;
    bool adbms_diag_fault;
    bool task_heartbeat_fault;
    bool logger_heartbeat_fault;
    bool compact_sequence_repeated;
    bool compact_sequence_fault;
    uint32_t compact_status_rx_count;
    uint32_t compact_sequence_error_count;
    uint32_t last_status_rx_tick;
    bool compact_status_stale;

    /* Compact 0x681 electrical frame. */
    uint16_t pack_voltage_0p1v;
    int16_t pack_current_0p1a;
    uint16_t min_cell_mv;
    uint16_t max_cell_mv;
    bool compact_electrical_valid;
    bool compact_electrical_sane;
    bool compact_electrical_stale;
    uint32_t last_electrical_rx_tick;
    uint32_t compact_electrical_sequence;

    /* Compact 0x682 thermal frame. */
    int16_t max_temp_0p1c;
    int16_t min_temp_0p1c;
    int16_t avg_temp_0p1c;
    uint8_t max_fan_percent;
    uint8_t thermal_flags;
    bool compact_thermal_valid;
    bool compact_thermal_sane;
    bool compact_thermal_stale;
    uint32_t last_thermal_rx_tick;

    /* Compact 0x683 health/location frame. */
    uint8_t max_voltage_segment;
    uint8_t max_voltage_cell;
    uint8_t min_voltage_segment;
    uint8_t min_voltage_cell;
    uint8_t max_temp_segment;
    uint8_t max_temp_sensor;
    uint8_t usable_cell_count;
    uint8_t usable_temp_count;
    bool compact_health_valid;
    bool compact_health_sane;
    bool compact_health_stale;
    uint32_t last_health_rx_tick;

    /* Advisory 0x68B canonical pack-current source diagnostics. Source ID is
     * never used to select a different torque model or grant authority. */
    uint8_t current_source;
    uint8_t current_quality;
    uint8_t current_boundary;
    uint8_t current_source_epoch;
    uint16_t current_sample_sequence_low;
    uint16_t current_sample_age_ms;
    uint32_t current_physical_sample_tick;
    bool current_diag_valid;
    bool current_diag_sane;
    uint32_t current_diag_rx_count;
    uint32_t last_current_diag_rx_tick;

    /* Authoritative dynamic power envelope (0x684-0x687) plus optional
     * strategy/resource (0x689) and binding metadata (0x68A). */
    der26_power_consumer_t power_consumer;
    der26_power_immediate_authority_t power_authority;
    bool power_authority_valid;
    bool power_authority_stale;

    uint32_t last_rx_tick;
    uint32_t rx_count;
    uint32_t bad_rx_count;
    uint16_t last_packet_header;
    bool stale;
} ams_t;

void segment_init(segment_t *dev);
void ams_init(ams_t *dev);
bool ams_parse_telemetry_frame(ams_t *dev, const uint8_t *data, uint8_t dlc, uint32_t now_ms);
bool ams_parse_estimator_frame(ams_t *dev, const uint8_t *data, uint8_t dlc, uint32_t now_ms);
bool ams_parse_can_frame(ams_t *dev, uint32_t std_id, bool is_standard, uint8_t dlc, const uint8_t *data, uint32_t now_ms);
bool ams_is_known_can_id(uint32_t std_id);
void ams_invalidate_can_frame(ams_t *dev, uint32_t std_id);
void ams_update_stale(ams_t *dev, uint32_t now_ms);
bool ams_allows_torque(const ams_t *dev);
bool ams_power_authority_allows_torque_command(
    const der26_power_immediate_authority_t *authority,
    int16_t torque_0p1nm);

bool ams_get_immediate_power_authority(const ams_t *dev,
                                       uint32_t now_ms,
                                       der26_power_immediate_authority_t *authority);
bool ams_get_feasibility_envelope(const ams_t *dev,
                                  uint32_t now_ms,
                                  der26_power_feasibility_envelope_t *envelope);
bool ams_get_power_resource_state(const ams_t *dev,
                                  uint32_t now_ms,
                                  der26_power_resource_state_t *resource);
bool ams_get_power_soh(const ams_t *dev,
                       uint32_t now_ms,
                       der26_power_soh_t *soh);
bool ams_encode_mission_request(uint8_t profile,
                                uint8_t counter,
                                bool stationary_confirmed,
                                uint8_t payload[8]);

#endif /* __AMS_H_ */
