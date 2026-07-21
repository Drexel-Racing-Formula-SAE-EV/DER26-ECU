/*
 * ams.c
 *
 *  Created on: Mar 28, 2024
 *      Author: cole
 */

#include <stddef.h>
#include <string.h>

#include "ext_drivers/ams.h"

typedef struct
{
    uint16_t header;
    uint16_t *d0;
    uint16_t *d1;
    uint16_t *d2;
} ams_packet_map_t;

static uint16_t u16_be(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8u) | (uint16_t)data[1]);
}

static int16_t s16_be(const uint8_t *data)
{
    return (int16_t)u16_be(data);
}

static bool bit_is_set(uint8_t value, uint8_t bit)
{
    return ((value & (uint8_t)(1u << bit)) != 0u);
}

static void mark_rx(ams_t *dev, uint32_t now_ms)
{
    dev->last_rx_tick = now_ms;
    dev->rx_count++;
}

static void refresh_compact_stale_flag(ams_t *dev)
{
    dev->stale = (!dev->compact_status_valid || dev->compact_status_stale ||
                  !dev->compact_electrical_valid || dev->compact_electrical_stale ||
                  !dev->compact_thermal_valid || dev->compact_thermal_stale);
}

void ams_invalidate_can_frame(ams_t *dev, uint32_t std_id)
{
    if(dev == NULL)
    {
        return;
    }

    switch(std_id)
    {
        case AMS_ECU_STATUS_CANBUS_ID:
            dev->compact_status_valid = false;
            dev->compact_protocol_valid = false;
            dev->compact_status_stale = true;
            break;
        case AMS_ECU_ELECTRICAL_CANBUS_ID:
            dev->compact_electrical_valid = false;
            dev->compact_electrical_sane = false;
            dev->compact_electrical_stale = true;
            break;
        case AMS_ECU_THERMAL_CANBUS_ID:
            dev->compact_thermal_valid = false;
            dev->compact_thermal_sane = false;
            dev->compact_thermal_stale = true;
            break;
        case AMS_ECU_HEALTH_CANBUS_ID:
            dev->compact_health_valid = false;
            dev->compact_health_sane = false;
            dev->compact_health_stale = true;
            break;
        default:
            break;
    }

    refresh_compact_stale_flag(dev);
}

void segment_init(segment_t *dev)
{
    if(dev == NULL)
    {
        return;
    }

    for(int i = 0; i < NVOLTS; i++)
    {
        dev->volts[i] = 0u;
    }

    for(int i = 0; i < NTEMPS; i++)
    {
        dev->temps[i] = 0u;
    }
}

void ams_init(ams_t *dev)
{
    if(dev == NULL)
    {
        return;
    }

    memset(dev, 0, sizeof(*dev));

    for(int i = 0; i < NSEGS; i++)
    {
        segment_init(&dev->segs[i]);
    }
}

static ams_packet_map_t ams_get_packet_map(ams_t *ams, uint16_t header)
{
    ams_packet_map_t pkt = { header, NULL, NULL, NULL };

    if(ams == NULL)
    {
        return pkt;
    }

    if(header <= 2u)
    {
        switch(header)
        {
            case 0u:
                pkt.d0 = &ams->state;
                pkt.d1 = &ams->air_state;
                pkt.d2 = &ams->current;
                break;
            case 1u:
                pkt.d0 = &ams->imd_ok;
                pkt.d1 = &ams->imd_status;
                pkt.d2 = &ams->imd_duty;
                break;
            case 2u:
                pkt.d0 = &ams->max_temp;
                pkt.d1 = &ams->min_volt;
                pkt.d2 = &ams->max_volt;
                break;
            default:
                break;
        }
        return pkt;
    }

    if((header >= 3u) && (header <= 27u))
    {
        uint16_t offset = (uint16_t)(header - 3u);
        uint16_t seg = (uint16_t)(offset / 5u);
        uint16_t group = (uint16_t)(offset % 5u);
        uint16_t base = (uint16_t)(group * 3u);

        if((seg < NSEGS) && ((base + 2u) < NVOLTS))
        {
            pkt.d0 = &ams->segs[seg].volts[base + 0u];
            pkt.d1 = &ams->segs[seg].volts[base + 1u];
            pkt.d2 = &ams->segs[seg].volts[base + 2u];
        }
        return pkt;
    }

    if((header >= 28u) && (header <= 57u))
    {
        uint16_t offset = (uint16_t)(header - 28u);
        uint16_t seg = (uint16_t)(offset / 6u);
        uint16_t group = (uint16_t)(offset % 6u);
        uint16_t base = (uint16_t)(group * 3u);

        if((seg < NSEGS) && (base < NTEMPS))
        {
            pkt.d0 = &ams->segs[seg].temps[base + 0u];
            if((base + 1u) < NTEMPS)
            {
                pkt.d1 = &ams->segs[seg].temps[base + 1u];
            }
            if((base + 2u) < NTEMPS)
            {
                pkt.d2 = &ams->segs[seg].temps[base + 2u];
            }
        }
        return pkt;
    }

    if((header >= 58u) && (header <= 61u))
    {
        uint16_t base = (uint16_t)((header - 58u) * 3u);
        if(base < NFANS)
        {
            pkt.d0 = &ams->fans[base + 0u];
        }
        if((base + 1u) < NFANS)
        {
            pkt.d1 = &ams->fans[base + 1u];
        }
        if((base + 2u) < NFANS)
        {
            pkt.d2 = &ams->fans[base + 2u];
        }
        return pkt;
    }

    return pkt;
}

bool ams_parse_telemetry_frame(ams_t *dev, const uint8_t *data, uint8_t dlc, uint32_t now_ms)
{
    if((dev == NULL) || (data == NULL) || (dlc != AMS_FRAME_DLC))
    {
        if(dev != NULL)
        {
            dev->bad_rx_count++;
        }
        return false;
    }

    uint16_t header = u16_be(&data[0]);
    uint16_t data0 = u16_be(&data[2]);
    uint16_t data1 = u16_be(&data[4]);
    uint16_t data2 = u16_be(&data[6]);

    if(header >= AMS_PACKET_COUNT)
    {
        dev->bad_rx_count++;
        return false;
    }

    ams_packet_map_t pkt = ams_get_packet_map(dev, header);
    if(pkt.header != header)
    {
        dev->bad_rx_count++;
        return false;
    }

    if(pkt.d0 != NULL)
    {
        *pkt.d0 = data0;
    }
    if(pkt.d1 != NULL)
    {
        *pkt.d1 = data1;
    }
    if(pkt.d2 != NULL)
    {
        *pkt.d2 = data2;
    }

    dev->last_packet_header = header;
    mark_rx(dev, now_ms);
    if(!dev->compact_status_valid)
    {
        dev->stale = false;
    }
    return true;
}

bool ams_parse_estimator_frame(ams_t *dev, const uint8_t *data, uint8_t dlc, uint32_t now_ms)
{
    if((dev == NULL) || (data == NULL) || (dlc != AMS_FRAME_DLC))
    {
        if(dev != NULL)
        {
            dev->bad_rx_count++;
        }
        return false;
    }

    for(uint8_t i = 0u; i < 4u; i++)
    {
        dev->estimator.words[i] = u16_be(&data[(uint8_t)(i * 2u)]);
    }

    dev->estimator.last_rx_tick = now_ms;
    dev->estimator.rx_count++;
    dev->estimator.valid = true;
    return true;
}

static bool ams_parse_compact_status_frame(ams_t *dev, const uint8_t *data, uint8_t dlc, uint32_t now_ms)
{
    uint8_t sequence;
    uint8_t expected;

    if((dev == NULL) || (data == NULL) || (dlc != AMS_FRAME_DLC))
    {
        if(dev != NULL)
        {
            dev->bad_rx_count++;
        }
        return false;
    }

    sequence = data[1];
    if(dev->compact_status_valid)
    {
        expected = (uint8_t)(dev->compact_sequence + 1u);
        dev->compact_sequence_repeated = (sequence == dev->compact_sequence);
        dev->compact_sequence_fault = (sequence != expected);
        if(dev->compact_sequence_fault)
        {
            dev->compact_sequence_error_count++;
        }
    }
    else
    {
        dev->compact_sequence_repeated = false;
        dev->compact_sequence_fault = false;
    }

    dev->compact_protocol_version = data[0];
    dev->compact_protocol_valid = (data[0] == AMS_ECU_COMPACT_PROTOCOL_VERSION);
    dev->compact_sequence = sequence;
    dev->compact_state = data[2];
    dev->compact_status_flags = data[3];
    dev->compact_fault_flags = data[4];
    dev->voltage_fault_reason = data[5];
    dev->temp_fault_reason = data[6];
    dev->current_fault_reason = data[7];

    dev->bms_ok = bit_is_set(data[3], 0u);
    dev->bms_inhibited = bit_is_set(data[3], 1u);
    dev->ams_hard_fault = bit_is_set(data[3], 2u);
    dev->ams_soft_fault = bit_is_set(data[3], 3u);
    dev->voltage_valid = bit_is_set(data[3], 4u);
    dev->current_valid = bit_is_set(data[3], 5u);
    dev->temp_valid = bit_is_set(data[3], 6u);
    dev->ams_can_fault = bit_is_set(data[3], 7u);

    dev->voltage_fault = bit_is_set(data[4], 0u);
    dev->temp_fault = bit_is_set(data[4], 1u);
    dev->current_fault = bit_is_set(data[4], 2u);
    /* Bit 3 is reserved until AMS firmware actually decodes IMD state. */
    dev->charger_fault = bit_is_set(data[4], 4u);
    dev->adbms_diag_fault = bit_is_set(data[4], 5u);
    dev->task_heartbeat_fault = bit_is_set(data[4], 6u);
    dev->logger_heartbeat_fault = bit_is_set(data[4], 7u);

    dev->compact_status_valid = true;
    dev->compact_status_rx_count++;
    dev->last_status_rx_tick = now_ms;
    dev->compact_status_stale = false;
    mark_rx(dev, now_ms);
    refresh_compact_stale_flag(dev);
    return true;
}

static bool ams_parse_compact_electrical_frame(ams_t *dev, const uint8_t *data, uint8_t dlc, uint32_t now_ms)
{
    if((dev == NULL) || (data == NULL) || (dlc != AMS_FRAME_DLC))
    {
        if(dev != NULL)
        {
            dev->bad_rx_count++;
        }
        return false;
    }

    dev->pack_voltage_0p1v = u16_be(&data[0]);
    dev->pack_current_0p1a = s16_be(&data[2]);
    dev->min_cell_mv = u16_be(&data[4]);
    dev->max_cell_mv = u16_be(&data[6]);
    dev->compact_electrical_valid = true;
    dev->compact_electrical_sane =
        ((dev->min_cell_mv >= AMS_CELL_VALID_MIN_MV) &&
         (dev->min_cell_mv <= dev->max_cell_mv) &&
         (dev->max_cell_mv <= AMS_CELL_VALID_MAX_MV) &&
         (dev->pack_voltage_0p1v <= AMS_PACK_VALID_MAX_0P1V) &&
         (dev->pack_current_0p1a >= AMS_CURRENT_VALID_MIN_0P1A) &&
         (dev->pack_current_0p1a <= AMS_CURRENT_VALID_MAX_0P1A));
    dev->compact_electrical_stale = false;
    dev->last_electrical_rx_tick = now_ms;
    mark_rx(dev, now_ms);
    refresh_compact_stale_flag(dev);
    return true;
}

static bool ams_parse_compact_thermal_frame(ams_t *dev, const uint8_t *data, uint8_t dlc, uint32_t now_ms)
{
    if((dev == NULL) || (data == NULL) || (dlc != AMS_FRAME_DLC))
    {
        if(dev != NULL)
        {
            dev->bad_rx_count++;
        }
        return false;
    }

    dev->max_temp_0p1c = s16_be(&data[0]);
    dev->min_temp_0p1c = s16_be(&data[2]);
    dev->avg_temp_0p1c = s16_be(&data[4]);
    dev->max_fan_percent = data[6];
    dev->thermal_flags = data[7];
    dev->compact_thermal_valid = true;
    dev->compact_thermal_sane = ((dev->min_temp_0p1c >= AMS_TEMP_VALID_MIN_0P1C) &&
                                 (dev->min_temp_0p1c <= dev->avg_temp_0p1c) &&
                                 (dev->avg_temp_0p1c <= dev->max_temp_0p1c) &&
                                 (dev->max_temp_0p1c <= AMS_TEMP_VALID_MAX_0P1C) &&
                                 (dev->max_fan_percent <= 100u));
    dev->compact_thermal_stale = false;
    dev->last_thermal_rx_tick = now_ms;
    mark_rx(dev, now_ms);
    refresh_compact_stale_flag(dev);
    return true;
}

static bool ams_parse_compact_health_frame(ams_t *dev, const uint8_t *data, uint8_t dlc, uint32_t now_ms)
{
    if((dev == NULL) || (data == NULL) || (dlc != AMS_FRAME_DLC))
    {
        if(dev != NULL)
        {
            dev->bad_rx_count++;
        }
        return false;
    }

    dev->max_voltage_segment = data[0];
    dev->max_voltage_cell = data[1];
    dev->min_voltage_segment = data[2];
    dev->min_voltage_cell = data[3];
    dev->max_temp_segment = data[4];
    dev->max_temp_sensor = data[5];
    dev->usable_cell_count = data[6];
    dev->usable_temp_count = data[7];
    dev->compact_health_valid = true;
    dev->compact_health_sane = ((dev->max_voltage_segment < NSEGS) &&
                                (dev->min_voltage_segment < NSEGS) &&
                                (dev->max_temp_segment < NSEGS) &&
                                (dev->max_voltage_cell < NVOLTS) &&
                                (dev->min_voltage_cell < NVOLTS) &&
                                (dev->max_temp_sensor < NTEMPS) &&
                                (dev->usable_cell_count <= AMS_TOTAL_CELL_COUNT) &&
                                (dev->usable_temp_count <= AMS_TOTAL_TEMP_COUNT));
    dev->compact_health_stale = false;
    dev->last_health_rx_tick = now_ms;
    mark_rx(dev, now_ms);
    return true;
}

bool ams_is_known_can_id(uint32_t std_id)
{
    return ((std_id == AMS_TELEM_CANBUS_ID) ||
            (std_id == AMS_ESTIMATOR_CANBUS_ID) ||
            (std_id == AMS_ECU_STATUS_CANBUS_ID) ||
            (std_id == AMS_ECU_ELECTRICAL_CANBUS_ID) ||
            (std_id == AMS_ECU_THERMAL_CANBUS_ID) ||
            (std_id == AMS_ECU_HEALTH_CANBUS_ID));
}

bool ams_parse_can_frame(ams_t *dev, uint32_t std_id, bool is_standard, uint8_t dlc, const uint8_t *data, uint32_t now_ms)
{
    if(dev == NULL)
    {
        return false;
    }

    if((data == NULL) || !is_standard ||
       (ams_is_known_can_id(std_id) && (dlc != AMS_FRAME_DLC)))
    {
        if(ams_is_known_can_id(std_id))
        {
            dev->bad_rx_count++;
            ams_invalidate_can_frame(dev, std_id);
        }
        return false;
    }

    if(std_id == AMS_TELEM_CANBUS_ID)
    {
        return ams_parse_telemetry_frame(dev, data, dlc, now_ms);
    }

    if(std_id == AMS_ESTIMATOR_CANBUS_ID)
    {
        return ams_parse_estimator_frame(dev, data, dlc, now_ms);
    }

    if(std_id == AMS_ECU_STATUS_CANBUS_ID)
    {
        return ams_parse_compact_status_frame(dev, data, dlc, now_ms);
    }

    if(std_id == AMS_ECU_ELECTRICAL_CANBUS_ID)
    {
        return ams_parse_compact_electrical_frame(dev, data, dlc, now_ms);
    }

    if(std_id == AMS_ECU_THERMAL_CANBUS_ID)
    {
        return ams_parse_compact_thermal_frame(dev, data, dlc, now_ms);
    }

    if(std_id == AMS_ECU_HEALTH_CANBUS_ID)
    {
        return ams_parse_compact_health_frame(dev, data, dlc, now_ms);
    }

    return false;
}

void ams_update_stale(ams_t *dev, uint32_t now_ms)
{
    if(dev == NULL)
    {
        return;
    }

    if(dev->compact_status_valid)
    {
        dev->compact_status_stale = ((uint32_t)(now_ms - dev->last_status_rx_tick) > AMS_STALE_TIMEOUT_MS);
        dev->compact_electrical_stale = (!dev->compact_electrical_valid ||
            ((uint32_t)(now_ms - dev->last_electrical_rx_tick) > AMS_STALE_TIMEOUT_MS));
        dev->compact_thermal_stale = (!dev->compact_thermal_valid ||
            ((uint32_t)(now_ms - dev->last_thermal_rx_tick) > AMS_STALE_TIMEOUT_MS));
        dev->compact_health_stale = (!dev->compact_health_valid ||
            ((uint32_t)(now_ms - dev->last_health_rx_tick) > AMS_STALE_TIMEOUT_MS));
        refresh_compact_stale_flag(dev);
        return;
    }

    if(dev->rx_count != 0u)
    {
        dev->stale = ((uint32_t)(now_ms - dev->last_rx_tick) > AMS_STALE_TIMEOUT_MS);
    }
    else
    {
        dev->stale = true;
    }
}

bool ams_allows_torque(const ams_t *dev)
{
    if(dev == NULL)
    {
        return false;
    }

    if(dev->compact_status_valid)
    {
        return (!dev->stale &&
                !dev->compact_status_stale &&
                dev->compact_electrical_valid &&
                dev->compact_electrical_sane &&
                !dev->compact_electrical_stale &&
                dev->compact_thermal_valid &&
                dev->compact_thermal_sane &&
                !dev->compact_thermal_stale &&
                ((dev->thermal_flags & AMS_THERMAL_TORQUE_BLOCK_MASK) == 0u) &&
                dev->bms_ok &&
                !dev->bms_inhibited &&
                !dev->ams_hard_fault &&
                !dev->ams_soft_fault &&
                dev->voltage_valid &&
                dev->current_valid &&
                dev->temp_valid &&
                !dev->ams_can_fault &&
                !dev->voltage_fault &&
                !dev->temp_fault &&
                !dev->current_fault &&
                !dev->charger_fault &&
                !dev->adbms_diag_fault &&
                !dev->task_heartbeat_fault &&
                !dev->logger_heartbeat_fault &&
                dev->compact_protocol_valid &&
                !dev->compact_sequence_fault);
    }

    return false;
}
