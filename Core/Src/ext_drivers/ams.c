/*
 * ams.c
 *
 *  Created on: Mar 28, 2024
 *      Author: cole
 */

#include <math.h>
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

static void refresh_power_authority_cache(ams_t *dev, uint32_t now_ms)
{
    if(dev == NULL)
    {
        return;
    }

    memset(&dev->power_authority, 0, sizeof(dev->power_authority));
    dev->power_authority_valid =
        der26_power_consumer_get_immediate_authority(
            &dev->power_consumer, now_ms, &dev->power_authority);
    dev->power_authority_stale = !dev->power_authority_valid;
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
        case AMS_ECU_CURRENT_DIAG_CANBUS_ID:
            /* Advisory-only diagnostics. Invalidating this frame must never
             * revoke an otherwise coherent AMS authority bundle. */
            dev->current_diag_valid = false;
            dev->current_diag_sane = false;
            break;
        case DER26_POWER_DCL_ID:
        case DER26_POWER_CCL_ID:
        case DER26_POWER_SOH_ID:
        case DER26_POWER_ENVELOPE_ID:
            der26_power_consumer_invalidate_id(&dev->power_consumer,
                                               (uint16_t)std_id);
            dev->power_authority_valid = false;
            dev->power_authority_stale = true;
            memset(&dev->power_authority, 0, sizeof(dev->power_authority));
            break;
        case DER26_POWER_STRATEGY_ID:
        case DER26_POWER_BINDINGS_ID:
            /* 0x689/0x68A are advisory.  A malformed optional frame must not
             * revoke an otherwise valid 0x684-0x687 authority bundle. */
            der26_power_consumer_invalidate_id(&dev->power_consumer,
                                               (uint16_t)std_id);
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
    der26_power_consumer_init(&dev->power_consumer);
    dev->power_authority_valid = false;
    dev->power_authority_stale = true;

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

    if((header >= 28u) && (header <= 67u))
    {
        uint16_t offset = (uint16_t)(header - 28u);
        uint16_t seg = (uint16_t)(offset / 8u);
        uint16_t group = (uint16_t)(offset % 8u);
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

    if((header >= 68u) && (header <= 71u))
    {
        uint16_t base = (uint16_t)((header - 68u) * 3u);
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
    if((pkt.header != header) ||
       ((pkt.d0 == NULL) && (pkt.d1 == NULL) && (pkt.d2 == NULL)))
    {
        /* A header inside the nominal packet-count range must still resolve
         * to at least one real destination.  This catches future layout/count
         * edits that otherwise would accept and silently discard a frame. */
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
    const uint32_t pack_voltage_mv =
        (uint32_t)dev->pack_voltage_0p1v * 100u;
    const uint32_t minimum_possible_pack_mv =
        (uint32_t)dev->min_cell_mv * AMS_TOTAL_CELL_COUNT;
    const uint32_t maximum_possible_pack_mv =
        (uint32_t)dev->max_cell_mv * AMS_TOTAL_CELL_COUNT;
    const bool pack_voltage_matches_cell_bounds =
        ((pack_voltage_mv + AMS_PACK_CELL_SUM_TOLERANCE_MV) >=
         minimum_possible_pack_mv) &&
        (pack_voltage_mv <=
         (maximum_possible_pack_mv + AMS_PACK_CELL_SUM_TOLERANCE_MV));

    dev->compact_electrical_valid = true;
    dev->compact_electrical_sane =
        ((dev->min_cell_mv >= AMS_CELL_VALID_MIN_MV) &&
         (dev->min_cell_mv <= dev->max_cell_mv) &&
         (dev->max_cell_mv <= AMS_CELL_VALID_MAX_MV) &&
         (dev->pack_voltage_0p1v <= AMS_PACK_VALID_MAX_0P1V) &&
         pack_voltage_matches_cell_bounds &&
         (dev->pack_current_0p1a >= AMS_CURRENT_VALID_MIN_0P1A) &&
         (dev->pack_current_0p1a <= AMS_CURRENT_VALID_MAX_0P1A));
    dev->compact_electrical_stale = false;
    dev->last_electrical_rx_tick = now_ms;
    if(dev->compact_electrical_sequence != UINT32_MAX)
    {
        dev->compact_electrical_sequence++;
    }
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

static bool ams_parse_current_diag_frame(ams_t *dev,
                                         const uint8_t *data,
                                         uint8_t dlc,
                                         uint32_t now_ms)
{
    if((dev == NULL) || (data == NULL) || (dlc != AMS_FRAME_DLC))
    {
        if(dev != NULL)
        {
            dev->bad_rx_count++;
        }
        return false;
    }

    dev->current_source = data[0];
    dev->current_quality = data[1];
    dev->current_boundary = data[2];
    dev->current_source_epoch = data[3];
    dev->current_sample_sequence_low = u16_be(&data[4]);
    dev->current_sample_age_ms = u16_be(&data[6]);
    dev->current_physical_sample_tick =
        now_ms - (uint32_t)dev->current_sample_age_ms;
    dev->current_diag_valid = true;
    dev->current_diag_sane =
        (dev->current_source <= 2u) &&
        (dev->current_quality <= 6u) &&
        (dev->current_boundary <= 1u) &&
        (dev->current_sample_age_ms <= AMS_STALE_TIMEOUT_MS) &&
        (((dev->current_source == 0u) &&
          (dev->current_quality == 0u) &&
          (dev->current_boundary == 0u)) ||
         ((dev->current_source != 0u) &&
          ((dev->current_quality == 2u) ||
           (dev->current_quality == 3u)) &&
          (dev->current_boundary == 1u)));
    if(dev->current_diag_rx_count != UINT32_MAX)
    {
        dev->current_diag_rx_count++;
    }
    dev->last_current_diag_rx_tick = now_ms;
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
            (std_id == AMS_ECU_HEALTH_CANBUS_ID) ||
            (std_id == AMS_ECU_CURRENT_DIAG_CANBUS_ID) ||
            (std_id == DER26_POWER_DCL_ID) ||
            (std_id == DER26_POWER_CCL_ID) ||
            (std_id == DER26_POWER_SOH_ID) ||
            (std_id == DER26_POWER_ENVELOPE_ID) ||
            (std_id == DER26_POWER_STRATEGY_ID) ||
            (std_id == DER26_POWER_BINDINGS_ID));
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

    if(std_id == AMS_ECU_CURRENT_DIAG_CANBUS_ID)
    {
        return ams_parse_current_diag_frame(dev, data, dlc, now_ms);
    }

    if((std_id == DER26_POWER_DCL_ID) ||
       (std_id == DER26_POWER_CCL_ID) ||
       (std_id == DER26_POWER_SOH_ID) ||
       (std_id == DER26_POWER_ENVELOPE_ID) ||
       (std_id == DER26_POWER_STRATEGY_ID) ||
       (std_id == DER26_POWER_BINDINGS_ID))
    {
        const bool accepted = der26_power_consumer_ingest(
            &dev->power_consumer, (uint16_t)std_id, false, false, dlc, data,
            now_ms);
        if((std_id >= DER26_POWER_DCL_ID) &&
           (std_id <= DER26_POWER_ENVELOPE_ID))
        {
            /* Keep the task-facing cache coherent immediately in the receive
             * path.  This makes a CRC/semantic failure fail closed without
             * waiting for the lower-rate error task, while a partial valid
             * replacement may continue using the previous bundle only for the
             * portable consumer's bounded 50 ms staging window. */
            refresh_power_authority_cache(dev, now_ms);
        }
        if(accepted)
        {
            mark_rx(dev, now_ms);
        }
        else
        {
            dev->bad_rx_count++;
        }
        return accepted;
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
    }
    else if(dev->rx_count != 0u)
    {
        dev->stale = ((uint32_t)(now_ms - dev->last_rx_tick) > AMS_STALE_TIMEOUT_MS);
    }
    else
    {
        dev->stale = true;
    }

    refresh_power_authority_cache(dev, now_ms);
    return;

}

static bool direction_authority_is_usable(
    const der26_power_direction_authority_t *direction)
{
    return (direction != NULL) &&
           (direction->authorized != 0u) &&
           isfinite(direction->current_limit_a) &&
           isfinite(direction->power_limit_w) &&
           (direction->current_limit_a > 0.0f) &&
           (direction->power_limit_w > 0.0f);
}

bool ams_power_authority_allows_torque_command(
    const der26_power_immediate_authority_t *authority,
    int16_t torque_0p1nm)
{
    const der26_power_direction_authority_t *direction;

    if((authority == NULL) || (authority->valid == 0u))
    {
        return false;
    }

    if(torque_0p1nm > 0)
    {
        direction = &authority->discharge;
    }
    else if(torque_0p1nm < 0)
    {
        direction = &authority->charge_regen;
    }
    else
    {
        /* An enabled zero-torque command is acceptable only while at least one
         * direction has real, nonzero AMS authority.  If both directions are
         * inhibited/fallback, send the explicit CM200 disable packet instead. */
        return direction_authority_is_usable(&authority->discharge) ||
               direction_authority_is_usable(&authority->charge_regen);
    }

    return direction_authority_is_usable(direction);
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
                !dev->compact_sequence_fault &&
#if AMS_POWER_AUTHORITY_REQUIRED_FOR_TORQUE
                dev->power_authority_valid &&
                !dev->power_authority_stale &&
                (ams_power_authority_allows_torque_command(
                     &dev->power_authority, 1) ||
                 ams_power_authority_allows_torque_command(
                     &dev->power_authority, -1))
#else
                true
#endif
                );
    }

    return false;
}

bool ams_get_immediate_power_authority(const ams_t *dev,
                                       uint32_t now_ms,
                                       der26_power_immediate_authority_t *authority)
{
    if((dev == NULL) || (authority == NULL))
    {
        return false;
    }
    return der26_power_consumer_get_immediate_authority(
        &dev->power_consumer, now_ms, authority);
}

bool ams_get_feasibility_envelope(const ams_t *dev,
                                  uint32_t now_ms,
                                  der26_power_feasibility_envelope_t *envelope)
{
    if((dev == NULL) || (envelope == NULL))
    {
        return false;
    }
    return der26_power_consumer_get_feasibility_envelope(
        &dev->power_consumer, now_ms, envelope);
}

bool ams_get_power_resource_state(const ams_t *dev,
                                  uint32_t now_ms,
                                  der26_power_resource_state_t *resource)
{
    if((dev == NULL) || (resource == NULL))
    {
        return false;
    }
    return der26_power_consumer_get_resource_state(
        &dev->power_consumer, now_ms, resource);
}

bool ams_get_power_soh(const ams_t *dev,
                       uint32_t now_ms,
                       der26_power_soh_t *soh)
{
    if((dev == NULL) || (soh == NULL))
    {
        return false;
    }
    return der26_power_consumer_get_soh(&dev->power_consumer, now_ms, soh);
}

bool ams_encode_mission_request(uint8_t profile,
                                uint8_t counter,
                                bool stationary_confirmed,
                                uint8_t payload[8])
{
    return der26_mission_request_encode(profile, counter,
                                        stationary_confirmed, payload);
}
