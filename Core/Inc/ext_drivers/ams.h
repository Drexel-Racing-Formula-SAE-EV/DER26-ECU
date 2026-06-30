/*
 * ams.h
 *
 *  Created on: Mar 28, 2024
 *      Author: cole
 */

#ifndef ECU_EXT_DRIVERS_AMS_H_
#define ECU_EXT_DRIVERS_AMS_H_

#include <stdbool.h>
#include <stdint.h>

#define NSEGS 5u
#define NFANS 10u
#define NVOLTS 15u
#define NTEMPS 17u

#define AMS_TELEM_CANBUS_ID 0x69u
#define AMS_ESTIMATOR_CANBUS_ID 0x421u
#define AMS_PACKET_COUNT 62u
#define AMS_FRAME_DLC 8u
#define AMS_STALE_TIMEOUT_MS 500u

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
void ams_update_stale(ams_t *dev, uint32_t now_ms);


#endif /* ECU_EXT_DRIVERS_AMS_H_ */
