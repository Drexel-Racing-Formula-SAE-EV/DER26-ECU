
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

void segment_init(segment_t *dev)
{
    if(dev == NULL)
    {
        return;
    }

    for(uint16_t i = 0u; i < NVOLTS; i++)
    {
        dev->volts[i] = 0u;
    }

    for(uint16_t i = 0u; i < NTEMPS; i++)
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

    for(uint16_t i = 0u; i < NSEGS; i++)
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
    dev->last_rx_tick = now_ms;
    dev->rx_count++;
    dev->stale = false;
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

bool ams_parse_can_frame(ams_t *dev, uint32_t std_id, bool is_standard, uint8_t dlc, const uint8_t *data, uint32_t now_ms)
{
    if((dev == NULL) || (data == NULL) || !is_standard)
    {
        if(dev != NULL)
        {
            dev->bad_rx_count++;
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

    return false;
}

void ams_update_stale(ams_t *dev, uint32_t now_ms)
{
    if(dev == NULL)
    {
        return;
    }

    if(dev->rx_count == 0u)
    {
        dev->stale = true;
        return;
    }

    dev->stale = ((uint32_t)(now_ms - dev->last_rx_tick) > AMS_STALE_TIMEOUT_MS);
}
