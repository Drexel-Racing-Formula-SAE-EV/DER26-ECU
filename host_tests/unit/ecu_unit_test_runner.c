#include "ext_drivers/ams.h"
#include "ext_drivers/cm200.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define FAIL_MSG(msg) do { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg)); failures++; } while(0)
#define EXPECT_TRUE(expr) do { if(!(expr)) { printf("FAIL %s:%d: expected true: %s\n", __FILE__, __LINE__, #expr); failures++; } } while(0)
#define EXPECT_FALSE(expr) EXPECT_TRUE(!(expr))
#define EXPECT_EQ_U16(actual, expected) do { uint16_t a_=(uint16_t)(actual); uint16_t e_=(uint16_t)(expected); if(a_ != e_) { printf("FAIL %s:%d: %s=%u expected=%u\n", __FILE__, __LINE__, #actual, (unsigned)a_, (unsigned)e_); failures++; } } while(0)
#define EXPECT_EQ_I16(actual, expected) do { int16_t a_=(int16_t)(actual); int16_t e_=(int16_t)(expected); if(a_ != e_) { printf("FAIL %s:%d: %s=%d expected=%d\n", __FILE__, __LINE__, #actual, (int)a_, (int)e_); failures++; } } while(0)
#define EXPECT_EQ_U8(actual, expected) do { uint8_t a_=(uint8_t)(actual); uint8_t e_=(uint8_t)(expected); if(a_ != e_) { printf("FAIL %s:%d: %s=%u expected=%u\n", __FILE__, __LINE__, #actual, (unsigned)a_, (unsigned)e_); failures++; } } while(0)
#define EXPECT_EQ_U32(actual, expected) do { uint32_t a_=(uint32_t)(actual); uint32_t e_=(uint32_t)(expected); if(a_ != e_) { printf("FAIL %s:%d: %s=%lu expected=%lu\n", __FILE__, __LINE__, #actual, (unsigned long)a_, (unsigned long)e_); failures++; } } while(0)
#define EXPECT_EQ_BOOL(actual, expected) do { bool a_=(bool)(actual); bool e_=(bool)(expected); if(a_ != e_) { printf("FAIL %s:%d: %s=%u expected=%u\n", __FILE__, __LINE__, #actual, (unsigned)a_, (unsigned)e_); failures++; } } while(0)

static void put_u16_be(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value >> 8u);
    dst[1] = (uint8_t)(value & 0xFFu);
}

static void put_u16_le(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)(value >> 8u);
}

static void put_u32_le(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8u) & 0xFFu);
    dst[2] = (uint8_t)((value >> 16u) & 0xFFu);
    dst[3] = (uint8_t)((value >> 24u) & 0xFFu);
}

static void feed_cm200_required_frames(cm200_t *cm, uint8_t counter, uint32_t now_ms)
{
    uint8_t a5[8] = {0};
    uint8_t a7[8] = {0};
    uint8_t aa[8] = {0};
    uint8_t ab[8] = {0};
    uint8_t ac[8] = {0};
    uint8_t b1[8] = {0};

    put_u16_le(&a5[2], 1500u);
    put_u16_le(&a7[0], 3200u);
    aa[0] = 5u;
    aa[2] = 1u;
    aa[5] = (uint8_t)(((counter + 1u) & 0x0Fu) << 4u);
    aa[7] = 0x01u;
    put_u32_le(&ac[4], 1000u);
    put_u16_le(&b1[0], 2000u);
    put_u16_le(&b1[2], 1500u);

    cm200_note_command_tx(cm, counter, false, 0, now_ms);
    EXPECT_TRUE(cm200_parse_can_frame(cm, CM200_MOTOR_POSITION_CAN_ID, true, 8u, a5, now_ms));
    EXPECT_TRUE(cm200_parse_can_frame(cm, CM200_VOLTAGE_CAN_ID, true, 8u, a7, now_ms));
    EXPECT_TRUE(cm200_parse_can_frame(cm, CM200_INTERNAL_STATES_CAN_ID, true, 8u, aa, now_ms));
    EXPECT_TRUE(cm200_parse_can_frame(cm, CM200_FAULTS_CAN_ID, true, 8u, ab, now_ms));
    EXPECT_TRUE(cm200_parse_can_frame(cm, CM200_TORQUE_TIMER_CAN_ID, true, 8u, ac, now_ms));
    EXPECT_TRUE(cm200_parse_can_frame(cm, CM200_TORQUE_CAP_CAN_ID, true, 8u, b1, now_ms));
    cm200_note_command_tx(cm, (uint8_t)((counter + 1u) & 0x0Fu), false, 0, now_ms + 5u);
    aa[5] = (uint8_t)(((counter + 2u) & 0x0Fu) << 4u);
    EXPECT_TRUE(cm200_parse_can_frame(cm,
                                      CM200_INTERNAL_STATES_CAN_ID,
                                      true,
                                      8u,
                                      aa,
                                      now_ms + 5u));
    put_u32_le(&ac[4], 1003u);
    EXPECT_TRUE(cm200_parse_can_frame(cm, CM200_TORQUE_TIMER_CAN_ID, true, 8u, ac, now_ms + 10u));
    cm200_update_stale(cm, now_ms + 10u);
}

static void make_packet(uint8_t frame[8], uint16_t header, uint16_t d0, uint16_t d1, uint16_t d2)
{
    put_u16_be(&frame[0], header);
    put_u16_be(&frame[2], d0);
    put_u16_be(&frame[4], d1);
    put_u16_be(&frame[6], d2);
}

static void feed_good_compact_summaries(ams_t *ams, uint32_t now_ms)
{
    const uint8_t electrical[8] = {0x0Bu, 0xB8u, 0x00u, 0x00u, 0x0Bu, 0xB8u, 0x10u, 0x04u};
    const uint8_t thermal[8] = {0x01u, 0x2Cu, 0x00u, 0xC8u, 0x00u, 0xFAu, 50u, 0u};
    EXPECT_TRUE(ams_parse_can_frame(ams, AMS_ECU_ELECTRICAL_CANBUS_ID, true, 8u, electrical, now_ms));
    EXPECT_TRUE(ams_parse_can_frame(ams, AMS_ECU_THERMAL_CANBUS_ID, true, 8u, thermal, now_ms));
}

static void poison_ams(ams_t *ams)
{
    memset(ams, 0xA5, sizeof(*ams));
}

static void test_segment_init_clears_all_fields(void)
{
    segment_t seg;
    memset(&seg, 0xFF, sizeof(seg));

    segment_init(&seg);

    for(uint16_t i = 0u; i < NVOLTS; i++)
    {
        EXPECT_EQ_U16(seg.volts[i], 0u);
    }

    for(uint16_t i = 0u; i < NTEMPS; i++)
    {
        EXPECT_EQ_U16(seg.temps[i], 0u);
    }

    segment_init(NULL);
}

static void test_ams_init_clears_poisoned_state(void)
{
    ams_t ams;
    poison_ams(&ams);
    ams_init(&ams);

    EXPECT_EQ_U16(NSEGS, 5u);
    EXPECT_EQ_U16(NVOLTS, 15u);
    EXPECT_EQ_U16(NTEMPS, 24u);
    EXPECT_EQ_U16(NFANS, 10u);
    EXPECT_EQ_U32(ams.rx_count, 0u);
    EXPECT_EQ_U32(ams.bad_rx_count, 0u);
    EXPECT_EQ_BOOL(ams.stale, false);
    EXPECT_EQ_BOOL(ams.estimator.valid, false);

    for(uint16_t s = 0u; s < NSEGS; s++)
    {
        for(uint16_t c = 0u; c < NVOLTS; c++)
        {
            EXPECT_EQ_U16(ams.segs[s].volts[c], 0u);
        }
        for(uint16_t t = 0u; t < NTEMPS; t++)
        {
            EXPECT_EQ_U16(ams.segs[s].temps[t], 0u);
        }
    }

    ams_init(NULL);
}

static void test_big_endian_status_packets(void)
{
    ams_t ams;
    uint8_t frame[8];
    ams_init(&ams);

    make_packet(frame, 0u, 0x1234u, 0xABCDu, 0x00EFu);
    EXPECT_TRUE(ams_parse_telemetry_frame(&ams, frame, 8u, 10u));
    EXPECT_EQ_U16(ams.state, 0x1234u);
    EXPECT_EQ_U16(ams.air_state, 0xABCDu);
    EXPECT_EQ_U16(ams.current, 0x00EFu);

    make_packet(frame, 1u, 0x0001u, 0x0203u, 0x0405u);
    EXPECT_TRUE(ams_parse_telemetry_frame(&ams, frame, 8u, 11u));
    EXPECT_EQ_U16(ams.imd_ok, 0x0001u);
    EXPECT_EQ_U16(ams.imd_status, 0x0203u);
    EXPECT_EQ_U16(ams.imd_duty, 0x0405u);

    make_packet(frame, 2u, 0x1111u, 0x2222u, 0x3333u);
    EXPECT_TRUE(ams_parse_telemetry_frame(&ams, frame, 8u, 12u));
    EXPECT_EQ_U16(ams.max_temp, 0x1111u);
    EXPECT_EQ_U16(ams.min_volt, 0x2222u);
    EXPECT_EQ_U16(ams.max_volt, 0x3333u);
    EXPECT_EQ_U16(ams.last_packet_header, 2u);
    EXPECT_EQ_U32(ams.last_rx_tick, 12u);
    EXPECT_EQ_U32(ams.rx_count, 3u);
}

static void test_all_voltage_packets_cover_75_cells(void)
{
    ams_t ams;
    uint8_t frame[8];
    ams_init(&ams);

    for(uint16_t header = 3u; header <= 27u; header++)
    {
        uint16_t offset = (uint16_t)(header - 3u);
        uint16_t seg = (uint16_t)(offset / 5u);
        uint16_t group = (uint16_t)(offset % 5u);
        uint16_t base = (uint16_t)(group * 3u);
        uint16_t v0 = (uint16_t)(1000u + (seg * 100u) + base + 0u);
        uint16_t v1 = (uint16_t)(1000u + (seg * 100u) + base + 1u);
        uint16_t v2 = (uint16_t)(1000u + (seg * 100u) + base + 2u);

        make_packet(frame, header, v0, v1, v2);
        EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_TELEM_CANBUS_ID, true, 8u, frame, (uint32_t)(100u + header)));
    }

    for(uint16_t seg = 0u; seg < NSEGS; seg++)
    {
        for(uint16_t cell = 0u; cell < NVOLTS; cell++)
        {
            uint16_t expected = (uint16_t)(1000u + (seg * 100u) + cell);
            EXPECT_EQ_U16(ams.segs[seg].volts[cell], expected);
        }
    }

    EXPECT_EQ_U32(ams.rx_count, 25u);
    EXPECT_EQ_U16(ams.last_packet_header, 27u);
}

static void test_all_temperature_packets_cover_tail_safely(void)
{
    ams_t ams;
    uint8_t frame[8];
    ams_init(&ams);

    for(uint16_t header = 28u; header <= 67u; header++)
    {
        uint16_t offset = (uint16_t)(header - 28u);
        uint16_t seg = (uint16_t)(offset / 8u);
        uint16_t group = (uint16_t)(offset % 8u);
        uint16_t base = (uint16_t)(group * 3u);
        uint16_t t0 = (uint16_t)(2000u + (seg * 100u) + base + 0u);
        uint16_t t1 = (uint16_t)(2000u + (seg * 100u) + base + 1u);
        uint16_t t2 = (uint16_t)(2000u + (seg * 100u) + base + 2u);

        make_packet(frame, header, t0, t1, t2);
        EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_TELEM_CANBUS_ID, true, 8u, frame, (uint32_t)(200u + header)));
    }

    for(uint16_t seg = 0u; seg < NSEGS; seg++)
    {
        for(uint16_t temp = 0u; temp < NTEMPS; temp++)
        {
            uint16_t expected = (uint16_t)(2000u + (seg * 100u) + temp);
            EXPECT_EQ_U16(ams.segs[seg].temps[temp], expected);
        }
    }

    EXPECT_EQ_U32(ams.rx_count, 40u);
    EXPECT_EQ_U16(ams.last_packet_header, 67u);
}

static void test_all_fan_packets_cover_only_10_fans(void)
{
    ams_t ams;
    uint8_t frame[8];
    ams_init(&ams);

    for(uint16_t header = 68u; header <= 71u; header++)
    {
        uint16_t base = (uint16_t)((header - 68u) * 3u);
        make_packet(frame, header,
                    (uint16_t)(3000u + base + 0u),
                    (uint16_t)(3000u + base + 1u),
                    (uint16_t)(3000u + base + 2u));
        EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_TELEM_CANBUS_ID, true, 8u, frame, (uint32_t)(300u + header)));
    }

    for(uint16_t fan = 0u; fan < NFANS; fan++)
    {
        EXPECT_EQ_U16(ams.fans[fan], (uint16_t)(3000u + fan));
    }

    EXPECT_EQ_U32(ams.rx_count, 4u);
    EXPECT_EQ_U16(ams.last_packet_header, 71u);
}

static void test_invalid_telemetry_frames_rejected_without_mutating_payload(void)
{
    ams_t ams;
    uint8_t frame[8];
    ams_init(&ams);

    make_packet(frame, 0u, 1u, 2u, 3u);
    EXPECT_TRUE(ams_parse_telemetry_frame(&ams, frame, 8u, 10u));
    EXPECT_EQ_U16(ams.state, 1u);
    EXPECT_EQ_U32(ams.rx_count, 1u);

    make_packet(frame, AMS_PACKET_COUNT, 9u, 9u, 9u);
    EXPECT_FALSE(ams_parse_telemetry_frame(&ams, frame, 8u, 11u));
    EXPECT_EQ_U16(ams.state, 1u);
    EXPECT_EQ_U32(ams.rx_count, 1u);
    EXPECT_EQ_U32(ams.bad_rx_count, 1u);

    make_packet(frame, 0u, 99u, 99u, 99u);
    EXPECT_FALSE(ams_parse_telemetry_frame(&ams, frame, 7u, 12u));
    EXPECT_EQ_U16(ams.state, 1u);
    EXPECT_EQ_U32(ams.rx_count, 1u);
    EXPECT_EQ_U32(ams.bad_rx_count, 2u);

    EXPECT_FALSE(ams_parse_telemetry_frame(&ams, NULL, 8u, 13u));
    EXPECT_EQ_U32(ams.bad_rx_count, 3u);
    EXPECT_FALSE(ams_parse_telemetry_frame(NULL, frame, 8u, 14u));
}

static void test_can_frame_filtering_and_bad_counts(void)
{
    ams_t ams;
    uint8_t frame[8];
    ams_init(&ams);
    make_packet(frame, 0u, 1u, 2u, 3u);

    EXPECT_FALSE(ams_parse_can_frame(&ams, 0x123u, true, 8u, frame, 1u));
    EXPECT_EQ_U32(ams.bad_rx_count, 0u);
    EXPECT_EQ_U32(ams.rx_count, 0u);

    EXPECT_FALSE(ams_parse_can_frame(&ams, AMS_TELEM_CANBUS_ID, false, 8u, frame, 2u));
    EXPECT_EQ_U32(ams.bad_rx_count, 1u);

    EXPECT_FALSE(ams_parse_can_frame(&ams, AMS_TELEM_CANBUS_ID, true, 8u, NULL, 3u));
    EXPECT_EQ_U32(ams.bad_rx_count, 2u);

    EXPECT_FALSE(ams_parse_can_frame(NULL, AMS_TELEM_CANBUS_ID, true, 8u, frame, 4u));
}

static void test_estimator_frame_storage_and_validation(void)
{
    ams_t ams;
    uint8_t frame[8];
    ams_init(&ams);

    make_packet(frame, 0x0102u, 0x0304u, 0x0506u, 0x0708u);
    EXPECT_TRUE(ams_parse_estimator_frame(&ams, frame, 8u, 55u));
    EXPECT_EQ_BOOL(ams.estimator.valid, true);
    EXPECT_EQ_U16(ams.estimator.words[0], 0x0102u);
    EXPECT_EQ_U16(ams.estimator.words[1], 0x0304u);
    EXPECT_EQ_U16(ams.estimator.words[2], 0x0506u);
    EXPECT_EQ_U16(ams.estimator.words[3], 0x0708u);
    EXPECT_EQ_U32(ams.estimator.last_rx_tick, 55u);
    EXPECT_EQ_U32(ams.estimator.rx_count, 1u);
    EXPECT_EQ_U32(ams.bad_rx_count, 0u);

    make_packet(frame, 0xFFFFu, 0xEEEEu, 0xDDDDu, 0xCCCCu);
    EXPECT_FALSE(ams_parse_estimator_frame(&ams, frame, 7u, 56u));
    EXPECT_EQ_U16(ams.estimator.words[0], 0x0102u);
    EXPECT_EQ_U32(ams.estimator.rx_count, 1u);
    EXPECT_EQ_U32(ams.bad_rx_count, 1u);

    EXPECT_FALSE(ams_parse_estimator_frame(&ams, NULL, 8u, 57u));
    EXPECT_EQ_U32(ams.bad_rx_count, 2u);
}

static void test_stale_state_and_wraparound(void)
{
    ams_t ams;
    uint8_t frame[8];
    ams_init(&ams);

    ams_update_stale(&ams, 0u);
    EXPECT_EQ_BOOL(ams.stale, true);

    make_packet(frame, 0u, 1u, 2u, 3u);
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_TELEM_CANBUS_ID, true, 8u, frame, 0xFFFFFF00u));
    EXPECT_EQ_BOOL(ams.stale, false);

    ams_update_stale(&ams, 0xFFFFFF00u + 500u);
    EXPECT_EQ_BOOL(ams.stale, false);

    ams_update_stale(&ams, 0xFFFFFF00u + 501u);
    EXPECT_EQ_BOOL(ams.stale, true);

    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_TELEM_CANBUS_ID, true, 8u, frame, 100u));
    ams_update_stale(&ams, 200u);
    EXPECT_EQ_BOOL(ams.stale, false);
    ams_update_stale(&ams, 701u);
    EXPECT_EQ_BOOL(ams.stale, true);
}

static void test_full_packet_sweep_counts_and_last_header(void)
{
    ams_t ams;
    uint8_t frame[8];
    ams_init(&ams);

    for(uint16_t header = 0u; header < AMS_PACKET_COUNT; header++)
    {
        make_packet(frame, header,
                    (uint16_t)(header + 1u),
                    (uint16_t)(header + 2u),
                    (uint16_t)(header + 3u));
        EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_TELEM_CANBUS_ID, true, 8u, frame, (uint32_t)header));
        EXPECT_EQ_U16(ams.last_packet_header, header);
    }

    EXPECT_EQ_U32(ams.rx_count, AMS_PACKET_COUNT);
    EXPECT_EQ_U32(ams.bad_rx_count, 0u);
    EXPECT_EQ_BOOL(ams.stale, false);
}


static void test_compact_status_frame_sets_torque_gate_fields(void)
{
    ams_t ams;
    uint8_t frame[8] = {
        AMS_ECU_COMPACT_PROTOCOL_VERSION,
        0x42u,
        3u,
        0x71u, /* BMS_OK + voltage/current/temp valid. */
        0x00u,
        0x11u,
        0x22u,
        0x33u
    };
    ams_init(&ams);

    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, frame, 100u));
    feed_good_compact_summaries(&ams, 100u);
    EXPECT_TRUE(ams.compact_status_valid);
    EXPECT_EQ_U8(ams.compact_protocol_version, AMS_ECU_COMPACT_PROTOCOL_VERSION);
    EXPECT_EQ_U8(ams.compact_sequence, 0x42u);
    EXPECT_EQ_U8(ams.compact_state, 3u);
    EXPECT_TRUE(ams.bms_ok);
    EXPECT_FALSE(ams.bms_inhibited);
    EXPECT_FALSE(ams.ams_hard_fault);
    EXPECT_FALSE(ams.ams_soft_fault);
    EXPECT_TRUE(ams.voltage_valid);
    EXPECT_TRUE(ams.current_valid);
    EXPECT_TRUE(ams.temp_valid);
    EXPECT_FALSE(ams.ams_can_fault);
    EXPECT_FALSE(ams.voltage_fault);
    EXPECT_FALSE(ams.temp_fault);
    EXPECT_FALSE(ams.current_fault);
    EXPECT_EQ_U8(ams.voltage_fault_reason, 0x11u);
    EXPECT_EQ_U8(ams.temp_fault_reason, 0x22u);
    EXPECT_EQ_U8(ams.current_fault_reason, 0x33u);
    EXPECT_EQ_U32(ams.last_status_rx_tick, 100u);
    EXPECT_EQ_U32(ams.rx_count, 3u);
    EXPECT_TRUE(ams_allows_torque(&ams));
}

static void test_compact_status_fault_flags_block_torque(void)
{
    ams_t ams;
    uint8_t frame[8] = {
        AMS_ECU_COMPACT_PROTOCOL_VERSION,
        1u,
        4u,
        0x71u,
        0x00u,
        0u,
        0u,
        0u
    };
    ams_init(&ams);

    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, frame, 10u));
    feed_good_compact_summaries(&ams, 10u);
    EXPECT_TRUE(ams_allows_torque(&ams));

    frame[1] = 2u;
    frame[3] = 0x70u; /* validity is present, BMS_OK is false. */
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, frame, 20u));
    EXPECT_FALSE(ams_allows_torque(&ams));

    frame[1] = 3u;
    frame[3] = 0x71u;
    frame[4] = 0x20u; /* ADBMS diagnostic fault. */
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, frame, 30u));
    EXPECT_TRUE(ams.adbms_diag_fault);
    EXPECT_FALSE(ams_allows_torque(&ams));

    frame[1] = 4u;
    frame[4] = 0x08u; /* Reserved IMD bit must not affect torque gate by itself. */
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, frame, 40u));
    EXPECT_TRUE(ams_allows_torque(&ams));
}

static void test_compact_electrical_thermal_and_health_frames(void)
{
    ams_t ams;
    uint8_t electrical[8] = {0x0Bu, 0xB8u, 0xFFu, 0x9Cu, 0x0Bu, 0xEAu, 0x10u, 0x68u};
    uint8_t thermal[8] = {0x01u, 0x59u, 0x00u, 0xF0u, 0x01u, 0x10u, 85u, 0xA5u};
    uint8_t health[8] = {4u, 14u, 0u, 2u, 3u, 23u, 75u, 120u};
    ams_init(&ams);

    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_ELECTRICAL_CANBUS_ID, true, 8u, electrical, 50u));
    EXPECT_TRUE(ams.compact_electrical_valid);
    EXPECT_EQ_U16(ams.pack_voltage_0p1v, 3000u);
    EXPECT_EQ_I16(ams.pack_current_0p1a, -100);
    EXPECT_EQ_U16(ams.min_cell_mv, 3050u);
    EXPECT_EQ_U16(ams.max_cell_mv, 4200u);

    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_THERMAL_CANBUS_ID, true, 8u, thermal, 60u));
    EXPECT_TRUE(ams.compact_thermal_valid);
    EXPECT_EQ_I16(ams.max_temp_0p1c, 345);
    EXPECT_EQ_I16(ams.min_temp_0p1c, 240);
    EXPECT_EQ_I16(ams.avg_temp_0p1c, 272);
    EXPECT_EQ_U8(ams.max_fan_percent, 85u);
    EXPECT_EQ_U8(ams.thermal_flags, 0xA5u);

    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_HEALTH_CANBUS_ID, true, 8u, health, 70u));
    EXPECT_TRUE(ams.compact_health_valid);
    EXPECT_EQ_U8(ams.max_voltage_segment, 4u);
    EXPECT_EQ_U8(ams.max_voltage_cell, 14u);
    EXPECT_EQ_U8(ams.min_voltage_segment, 0u);
    EXPECT_EQ_U8(ams.min_voltage_cell, 2u);
    EXPECT_EQ_U8(ams.max_temp_segment, 3u);
    EXPECT_EQ_U8(ams.max_temp_sensor, 23u);
    EXPECT_EQ_U8(ams.usable_cell_count, 75u);
    EXPECT_EQ_U8(ams.usable_temp_count, 120u);
    EXPECT_EQ_U32(ams.rx_count, 3u);
}

static void test_current_source_diagnostic_frame_is_advisory(void)
{
    ams_t ams;
    uint8_t diagnostic[8] = {1u, 2u, 1u, 5u, 0u, 9u, 0u, 7u};
    ams_init(&ams);

    EXPECT_TRUE(ams_is_known_can_id(AMS_ECU_CURRENT_DIAG_CANBUS_ID));
    EXPECT_TRUE(ams_parse_can_frame(&ams,
                                    AMS_ECU_CURRENT_DIAG_CANBUS_ID,
                                    true,
                                    8u,
                                    diagnostic,
                                    80u));
    EXPECT_TRUE(ams.current_diag_valid);
    EXPECT_TRUE(ams.current_diag_sane);
    EXPECT_EQ_U8(ams.current_source, 1u);
    EXPECT_EQ_U8(ams.current_quality, 2u);
    EXPECT_EQ_U8(ams.current_source_epoch, 5u);
    EXPECT_EQ_U16(ams.current_sample_sequence_low, 9u);
    EXPECT_EQ_U16(ams.current_sample_age_ms, 7u);
    EXPECT_EQ_U32(ams.current_physical_sample_tick, 73u);
    EXPECT_EQ_U8(ams.current_boundary, 1u);

    /* Malformed semantic content is retained for diagnostics but cannot
     * become a source of torque authority. */
    diagnostic[0] = 3u;
    EXPECT_TRUE(ams_parse_can_frame(&ams,
                                    AMS_ECU_CURRENT_DIAG_CANBUS_ID,
                                    true,
                                    8u,
                                    diagnostic,
                                    81u));
    EXPECT_TRUE(ams.current_diag_valid);
    EXPECT_FALSE(ams.current_diag_sane);

    ams_invalidate_can_frame(&ams, AMS_ECU_CURRENT_DIAG_CANBUS_ID);
    EXPECT_FALSE(ams.current_diag_valid);
    EXPECT_FALSE(ams.current_diag_sane);
}

static void test_logger_stream_frames_are_decoded_and_advisory(void)
{
    ams_t ams;
    uint8_t heartbeat[8] = {3u, 17u, 0u, 0u, 0u, 0u, 0u, 0u};
    uint8_t meta[8] = {2u, 9u, 4u, 6u, 0x01u, 0x02u, 0x03u, 0x04u};
    uint8_t sample[8] = {0x01u, 0xF4u, 0xFFu, 0x38u,
                         0x02u, 0x58u, 0x02u, 0x62u};
    uint8_t health[8] = {0xA5u, 7u, 3u, 6u, 0x12u, 0x34u, 0x00u, 0x64u};
    uint8_t raw[8] = {0xFFu, 0xFFu, 0xFCu, 0x18u,
                      0x01u, 0x2Cu, 5u, 1u};
    uint8_t txsched[8] = {0x00u, 0x02u, 0x00u, 0x07u,
                          0x00u, 0x03u, 4u, 0x03u};
    uint8_t vcompare[8] = {2u, 0x17u, 0x00u, 0x0Cu,
                           0xFFu, 0xE7u, 0x56u, 0x78u};

    ams_init(&ams);
    bool torque_before = ams_allows_torque(&ams);

    EXPECT_TRUE(ams_is_logger_can_id(AMS_LOGGER_CAN_ID_HEARTBEAT));
    EXPECT_TRUE(ams_is_logger_can_id(AMS_LOGGER_CAN_ID_APM_RAW));
    EXPECT_FALSE(ams_is_logger_can_id(AMS_LOGGER_CAN_ID_FIRST - 1u));
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_LOGGER_CAN_ID_HEARTBEAT,
                                    true, 8u, heartbeat, 100u));
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_LOGGER_CAN_ID_SNAPSHOT_META,
                                    true, 8u, meta, 101u));
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_LOGGER_CAN_ID_APM_SAMPLE,
                                    true, 8u, sample, 102u));
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_LOGGER_CAN_ID_APM_HEALTH,
                                    true, 8u, health, 103u));
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_LOGGER_CAN_ID_APM_RAW,
                                    true, 8u, raw, 104u));
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_LOGGER_CAN_ID_TX_SCHED_DIAG,
                                    true, 8u, txsched, 105u));
    EXPECT_TRUE(ams_parse_can_frame(&ams,
                                    AMS_LOGGER_CAN_ID_ESTIMATOR_VOLTAGE_COMPARE,
                                    true, 8u, vcompare, 106u));

    EXPECT_EQ_U32(ams.logger_protocol_version, 3u);
    EXPECT_EQ_U32(ams.logger_sequence, 17u);
    EXPECT_EQ_U32(ams.logger_snapshot_sequence, 9u);
    EXPECT_EQ_U32(ams.logger_phase, 4u);
    EXPECT_EQ_U32(ams.logger_phase_count, 6u);
    EXPECT_EQ_U32(ams.logger_measurement_sequence, 0x01020304u);
    EXPECT_EQ_I16(ams.apm_current1_0p01a, 500);
    EXPECT_EQ_I16(ams.apm_current2_0p01a, -200);
    EXPECT_EQ_U32(ams.apm_voltage1_0p1v, 600u);
    EXPECT_EQ_U32(ams.apm_voltage2_0p1v, 610u);
    EXPECT_EQ_U32(ams.apm_flags, 0xA5u);
    EXPECT_EQ_U32(ams.apm_conversion_count, 0x1234u);
    EXPECT_EQ_U32(ams.apm_sample_age_ms, 100u);
    EXPECT_TRUE(ams.apm_i1_raw == -1000);
    EXPECT_EQ_I16(ams.apm_vb1_raw, 300);
    EXPECT_EQ_U32(ams.apm_i1_phase, 5u);
    EXPECT_EQ_U32(ams.apm_calibration_profile, 1u);
    EXPECT_EQ_U32(ams.tx_sched_protected_deadline_miss, 2u);
    EXPECT_EQ_U32(ams.tx_sched_detail_superseded, 7u);
    EXPECT_EQ_U32(ams.tx_sched_detail_recovery_discard, 3u);
    EXPECT_EQ_U32(ams.tx_sched_protected_superseded, 4u);
    EXPECT_EQ_U32(ams.tx_sched_flags, 0x03u);
    EXPECT_EQ_U32(ams.estimator_voltage_compare_index, 2u);
    EXPECT_EQ_U32(ams.estimator_voltage_compare_flags, 0x17u);
    EXPECT_EQ_I16(ams.estimator_avg_minus_raw_mv, 12);
    EXPECT_EQ_I16(ams.estimator_iir_minus_raw_mv, -25);
    EXPECT_EQ_U32(ams.estimator_voltage_compare_sequence, 0x5678u);
    EXPECT_EQ_U32(ams.logger_rx_count, 7u);
    EXPECT_EQ_U32(ams.logger_last_id,
                  AMS_LOGGER_CAN_ID_ESTIMATOR_VOLTAGE_COMPARE);
    EXPECT_EQ_U32(ams.logger_last_rx_tick, 106u);
    EXPECT_EQ_U32(ams_allows_torque(&ams), torque_before);

    EXPECT_FALSE(ams_parse_can_frame(&ams, AMS_LOGGER_CAN_ID_APM_SAMPLE,
                                     true, 7u, sample, 107u));
    EXPECT_EQ_U32(ams.bad_rx_count, 1u);
    EXPECT_EQ_I16(ams.apm_current1_0p01a, 500);
}

static void test_logger_snapshot_gap_and_fragment_observability(void)
{
    ams_t ams;
    uint8_t meta1[8] = {AMS_LOGGER_SNAPSHOT_VERSION_V4, 1u, 0u, NSEGS,
                        0u, 0u, 0u, 1u};
    uint8_t meta3[8] = {AMS_LOGGER_SNAPSHOT_VERSION_V4, 3u, 0u, NSEGS,
                        0u, 0u, 0u, 3u};
    uint8_t cell1[8] = {0};
    uint8_t old_cell[8] = {0};

    ams_init(&ams);
    cell1[0] = (uint8_t)((1u << AMS_LOGGER_PHASE_BITS) | 0u);
    cell1[1] = 0u;
    old_cell[0] = cell1[0];
    old_cell[1] = 3u;

    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_LOGGER_CAN_ID_SNAPSHOT_META,
                                    true, 8u, meta1, 10u));
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_LOGGER_CAN_ID_CELL_DETAIL,
                                    true, 8u, cell1, 11u));
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_LOGGER_CAN_ID_CELL_DETAIL,
                                    true, 8u, cell1, 12u));
    EXPECT_EQ_U32(ams.logger_duplicate_fragment_count, 1u);

    /* Advancing 1 -> 3 records one missing source snapshot and finalizes the
     * incomplete fragment set for snapshot 1 before starting snapshot 3. */
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_LOGGER_CAN_ID_SNAPSHOT_META,
                                    true, 8u, meta3, 20u));
    EXPECT_EQ_U32(ams.logger_snapshot_gap_count, 1u);
    EXPECT_EQ_U32(ams.logger_incomplete_snapshot_count, 1u);
    EXPECT_EQ_U32(ams.logger_phase_gap_count, 4u);
    EXPECT_EQ_U32(ams.logger_cell_fragment_gap_count, 24u);
    EXPECT_EQ_U32(ams.logger_temp_fragment_gap_count, 40u);
    EXPECT_FALSE(ams.logger_snapshot_complete);

    /* A late modulo-32 fragment from snapshot 1 is observable but cannot
     * roll the current snapshot tracker backward from snapshot 3. */
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_LOGGER_CAN_ID_CELL_DETAIL,
                                    true, 8u, old_cell, 21u));
    EXPECT_EQ_U32(ams.logger_out_of_order_count, 1u);
    EXPECT_EQ_U8(ams.logger_fragment_seq5, 3u);
}

static void test_compact_pack_voltage_matches_cell_bounds(void)
{
    ams_t ams;
    uint8_t electrical[8] = {0x0Bu, 0xB8u, 0u, 0u, 0x0Bu, 0xB8u, 0x10u, 0x04u};
    ams_init(&ams);

    EXPECT_TRUE(ams_parse_can_frame(&ams,
                                    AMS_ECU_ELECTRICAL_CANBUS_ID,
                                    true,
                                    8u,
                                    electrical,
                                    10u));
    EXPECT_TRUE(ams.compact_electrical_sane);

    /* A zero or implausibly high pack summary cannot be reconciled with 75
     * cells bounded by the transmitted minimum and maximum.  The frame is
     * received for diagnostics, but must not become trusted torque-gate data. */
    electrical[0] = 0u;
    electrical[1] = 0u;
    EXPECT_TRUE(ams_parse_can_frame(&ams,
                                    AMS_ECU_ELECTRICAL_CANBUS_ID,
                                    true,
                                    8u,
                                    electrical,
                                    11u));
    EXPECT_TRUE(ams.compact_electrical_valid);
    EXPECT_FALSE(ams.compact_electrical_sane);

    electrical[0] = 0x0Fu;
    electrical[1] = 0xA0u; /* 400.0 V > 75 * 4.100 V. */
    EXPECT_TRUE(ams_parse_can_frame(&ams,
                                    AMS_ECU_ELECTRICAL_CANBUS_ID,
                                    true,
                                    8u,
                                    electrical,
                                    12u));
    EXPECT_FALSE(ams.compact_electrical_sane);
}

static void test_compact_status_drives_stale_and_sequence_checks(void)
{
    ams_t ams;
    uint8_t status[8] = {AMS_ECU_COMPACT_PROTOCOL_VERSION, 10u, 1u, 0x71u, 0u, 0u, 0u, 0u};
    uint8_t elec[8] = {0u};
    ams_init(&ams);

    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, status, 1000u));
    feed_good_compact_summaries(&ams, 1000u);
    EXPECT_TRUE(ams_allows_torque(&ams));

    ams_update_stale(&ams, 1500u);
    EXPECT_FALSE(ams.stale);
    ams_update_stale(&ams, 1501u);
    EXPECT_TRUE(ams.stale);
    EXPECT_FALSE(ams_allows_torque(&ams));

    /* Non-status compact frames must not refresh the status heartbeat once status exists. */
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_ELECTRICAL_CANBUS_ID, true, 8u, elec, 1600u));
    ams_update_stale(&ams, 1600u);
    EXPECT_TRUE(ams.stale);

    status[1] = 11u;
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, status, 1601u));
    feed_good_compact_summaries(&ams, 1601u);
    EXPECT_FALSE(ams.compact_sequence_repeated);
    EXPECT_TRUE(ams_allows_torque(&ams));

    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, status, 1700u));
    EXPECT_TRUE(ams.compact_sequence_repeated);
    EXPECT_FALSE(ams_allows_torque(&ams));
    EXPECT_EQ_U32(ams.compact_sequence_error_count, 1u);
}

static void test_compact_summary_freshness_thermal_and_sanity_gates(void)
{
    ams_t ams;
    uint8_t status[8] = {AMS_ECU_COMPACT_PROTOCOL_VERSION, 1u, 1u, 0x71u, 0u, 0u, 0u, 0u};
    uint8_t electrical[8] = {0x0Bu, 0xB8u, 0u, 0u, 0x10u, 0u, 0x0Fu, 0u};
    uint8_t thermal[8] = {0u, 100u, 0u, 50u, 0u, 75u, 50u, 0u};
    ams_init(&ams);

    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, status, 0u));
    feed_good_compact_summaries(&ams, 0u);
    EXPECT_TRUE(ams_allows_torque(&ams));

    status[1] = 2u;
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, status, 501u));
    ams_update_stale(&ams, 501u);
    EXPECT_FALSE(ams.compact_status_stale);
    EXPECT_TRUE(ams.compact_electrical_stale);
    EXPECT_TRUE(ams.compact_thermal_stale);
    EXPECT_FALSE(ams_allows_torque(&ams));

    /* An inverted min/max electrical summary is received but not trusted. */
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_ELECTRICAL_CANBUS_ID, true, 8u, electrical, 501u));
    EXPECT_FALSE(ams.compact_electrical_sane);
    EXPECT_FALSE(ams_allows_torque(&ams));

    feed_good_compact_summaries(&ams, 502u);
    EXPECT_TRUE(ams_allows_torque(&ams));

    thermal[7] = (uint8_t)(1u << AMS_THERMAL_OVERTEMP_FAULT_BIT);
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_THERMAL_CANBUS_ID, true, 8u, thermal, 503u));
    EXPECT_FALSE(ams_allows_torque(&ams));

    /* Warning, fan-max, charge-stop, pending, and fan diagnostic bits do not
     * by themselves block motoring torque under the AMS contract. */
    thermal[7] = 0x4Fu;
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_THERMAL_CANBUS_ID, true, 8u, thermal, 504u));
    EXPECT_TRUE(ams_allows_torque(&ams));

    thermal[6] = 101u;
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_THERMAL_CANBUS_ID, true, 8u, thermal, 505u));
    EXPECT_FALSE(ams.compact_thermal_sane);
    EXPECT_FALSE(ams_allows_torque(&ams));
}

static void test_compact_invalid_frames_rejected_without_mutation(void)
{
    ams_t ams;
    uint8_t frame[8] = {AMS_ECU_COMPACT_PROTOCOL_VERSION, 1u, 1u, 0x71u, 0u, 0u, 0u, 0u};
    ams_init(&ams);

    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, frame, 1u));
    EXPECT_EQ_U32(ams.rx_count, 1u);
    EXPECT_EQ_U8(ams.compact_sequence, 1u);

    frame[1] = 2u;
    EXPECT_FALSE(ams_parse_can_frame(&ams, AMS_ECU_STATUS_CANBUS_ID, true, 7u, frame, 2u));
    EXPECT_EQ_U32(ams.rx_count, 1u);
    EXPECT_EQ_U32(ams.bad_rx_count, 1u);
    EXPECT_EQ_U8(ams.compact_sequence, 1u);

    EXPECT_FALSE(ams_parse_can_frame(&ams, AMS_ECU_ELECTRICAL_CANBUS_ID, false, 8u, frame, 3u));
    EXPECT_EQ_U32(ams.bad_rx_count, 2u);
}

static void test_invalid_required_ams_frame_revokes_torque_immediately(void)
{
    ams_t ams;
    uint8_t status[8] = {AMS_ECU_COMPACT_PROTOCOL_VERSION, 1u, 1u, 0x71u, 0u, 0u, 0u, 0u};
    uint8_t electrical[8] = {0x0Bu, 0xB8u, 0u, 0u, 0x0Bu, 0xB8u, 0x10u, 0x04u};

    ams_init(&ams);
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, status, 10u));
    feed_good_compact_summaries(&ams, 10u);
    EXPECT_TRUE(ams_allows_torque(&ams));

    EXPECT_FALSE(ams_parse_can_frame(&ams,
                                     AMS_ECU_ELECTRICAL_CANBUS_ID,
                                     true,
                                     7u,
                                     electrical,
                                     11u));
    EXPECT_FALSE(ams.compact_electrical_valid);
    EXPECT_TRUE(ams.compact_electrical_stale);
    EXPECT_FALSE(ams_allows_torque(&ams));
}

static void test_power_authority_direction_gate(void)
{
    der26_power_immediate_authority_t authority;
    memset(&authority, 0, sizeof(authority));

    EXPECT_FALSE(ams_power_authority_allows_torque_command(NULL, 100));
    EXPECT_FALSE(ams_power_authority_allows_torque_command(&authority, 100));

    authority.valid = 1u;
    authority.discharge.authorized = 1u;
    authority.discharge.current_limit_a = 80.0f;
    authority.discharge.power_limit_w = 20000.0f;
    EXPECT_TRUE(ams_power_authority_allows_torque_command(&authority, 100));
    EXPECT_FALSE(ams_power_authority_allows_torque_command(&authority, -100));
    EXPECT_TRUE(ams_power_authority_allows_torque_command(&authority, 0));

    authority.discharge.authorized = 0u;
    authority.discharge.current_limit_a = 0.0f;
    authority.discharge.power_limit_w = 0.0f;
    authority.charge_regen.authorized = 1u;
    authority.charge_regen.current_limit_a = 10.0f;
    authority.charge_regen.power_limit_w = 2500.0f;
    EXPECT_FALSE(ams_power_authority_allows_torque_command(&authority, 100));
    EXPECT_TRUE(ams_power_authority_allows_torque_command(&authority, -100));
    EXPECT_TRUE(ams_power_authority_allows_torque_command(&authority, 0));

    authority.charge_regen.power_limit_w = 0.0f;
    EXPECT_FALSE(ams_power_authority_allows_torque_command(&authority, -100));
    EXPECT_FALSE(ams_power_authority_allows_torque_command(&authority, 0));

    authority.charge_regen.power_limit_w = INFINITY;
    EXPECT_FALSE(ams_power_authority_allows_torque_command(&authority, -100));
    authority.charge_regen.power_limit_w = NAN;
    EXPECT_FALSE(ams_power_authority_allows_torque_command(&authority, -100));
}

static void test_cm200_broadcast_decoding_and_required_gate(void)
{
    cm200_t cm;
    uint8_t a0[8] = {0};
    uint8_t a1[8] = {0};
    uint8_t a2[8] = {0};
    uint8_t a6[8] = {0};
    uint8_t ae[8] = {0};
    uint8_t aa[8] = {0};

    cm200_init(&cm);
    EXPECT_TRUE(cm.frame[CM200_FRAME_INTERNAL_STATES].stale);
    EXPECT_TRUE(cm200_is_known_can_id(CM200_INTERNAL_STATES_CAN_ID));
    EXPECT_FALSE(cm200_is_known_can_id(0x123u));

    put_u16_le(&a0[0], 250u);
    put_u16_le(&a0[2], (uint16_t)-100);
    put_u16_le(&a0[4], 300u);
    put_u16_le(&a0[6], 400u);
    put_u16_le(&a1[0], 260u);
    put_u16_le(&a1[2], 270u);
    put_u16_le(&a1[4], 280u);
    put_u16_le(&a1[6], 290u);
    put_u16_le(&a2[0], 300u);
    put_u16_le(&a2[2], 310u);
    put_u16_le(&a2[4], 320u);
    put_u16_le(&a2[6], 5u);
    put_u16_le(&a6[0], 100u);
    put_u16_le(&a6[2], 200u);
    put_u16_le(&a6[4], 300u);
    put_u16_le(&a6[6], (uint16_t)-50);
    put_u16_le(&ae[0], 0x1234u);
    put_u16_le(&ae[2], 0x5678u);
    put_u16_le(&ae[4], 0x0721u);
    put_u16_le(&ae[6], 2026u);

    EXPECT_TRUE(cm200_parse_can_frame(&cm, CM200_TEMPERATURES_1_CAN_ID, true, 8u, a0, 1u));
    EXPECT_TRUE(cm200_parse_can_frame(&cm, CM200_TEMPERATURES_2_CAN_ID, true, 8u, a1, 2u));
    EXPECT_TRUE(cm200_parse_can_frame(&cm, CM200_TEMPERATURES_3_CAN_ID, true, 8u, a2, 3u));
    EXPECT_TRUE(cm200_parse_can_frame(&cm, CM200_CURRENT_CAN_ID, true, 8u, a6, 4u));
    EXPECT_TRUE(cm200_parse_can_frame(&cm, CM200_FIRMWARE_CAN_ID, true, 8u, ae, 5u));
    EXPECT_EQ_I16(cm.module_a_temp_0p1c, 250);
    EXPECT_EQ_I16(cm.module_b_temp_0p1c, -100);
    EXPECT_EQ_I16(cm.dc_bus_current_0p1a, -50);
    EXPECT_EQ_U16(cm.eeprom_project_code, 0x1234u);
    EXPECT_EQ_U16(cm.software_version, 0x5678u);
    EXPECT_EQ_U16(cm.date_year, 2026u);
    EXPECT_FALSE(cm200_allows_torque(&cm));

    feed_cm200_required_frames(&cm, 3u, 100u);
    EXPECT_EQ_I16(cm.motor_speed_rpm, 1500);
    EXPECT_EQ_I16(cm.dc_bus_voltage_0p1v, 3200);
    EXPECT_EQ_I16(cm.motor_torque_available_0p1nm, 2000);
    EXPECT_TRUE(cm.counter_synced);
    EXPECT_TRUE(cm.torque_echo_synced);
    EXPECT_TRUE(cm.timer_observed_progress);
    EXPECT_TRUE(cm200_feedback_healthy(&cm));
    EXPECT_TRUE(cm200_allows_torque(&cm));
    EXPECT_EQ_I16(cm200_clamp_motoring_torque(&cm, 1500), 1500);
    EXPECT_EQ_I16(cm200_clamp_motoring_torque(&cm, 2500), 2000);

    /* Communications may be fully healthy during precharge, but VSM state 2
     * cannot authorize RTD/torque.  This split prevents a Firmware_OK deadlock. */
    aa[0] = 2u;
    aa[2] = 1u;
    aa[5] = 0x50u;
    aa[7] = 0x01u;
    EXPECT_TRUE(cm200_parse_can_frame(&cm, CM200_INTERNAL_STATES_CAN_ID, true, 8u, aa, 111u));
    EXPECT_TRUE(cm200_feedback_healthy(&cm));
    EXPECT_FALSE(cm200_allows_torque(&cm));
    aa[0] = 5u;
    EXPECT_TRUE(cm200_parse_can_frame(&cm, CM200_INTERNAL_STATES_CAN_ID, true, 8u, aa, 112u));
    EXPECT_TRUE(cm200_allows_torque(&cm));
}

static void test_cm200_malformed_and_freshness_fail_closed(void)
{
    cm200_t cm;
    uint8_t voltage[8] = {0};

    cm200_init(&cm);
    feed_cm200_required_frames(&cm, 1u, 100u);
    EXPECT_TRUE(cm200_allows_torque(&cm));

    put_u16_le(&voltage[0], 3200u);
    EXPECT_FALSE(cm200_parse_can_frame(&cm,
                                       CM200_VOLTAGE_CAN_ID,
                                       true,
                                       7u,
                                       voltage,
                                       111u));
    EXPECT_FALSE(cm.frame[CM200_FRAME_VOLTAGE].valid);
    EXPECT_FALSE(cm200_allows_torque(&cm));
    EXPECT_EQ_U32(cm.bad_rx_count, 1u);

    EXPECT_TRUE(cm200_parse_can_frame(&cm, CM200_VOLTAGE_CAN_ID, true, 8u, voltage, 112u));
    cm200_update_stale(&cm, 205u);
    EXPECT_FALSE(cm.command_tx_stale);
    cm200_update_stale(&cm, 206u);
    EXPECT_TRUE(cm.command_tx_stale);
    EXPECT_FALSE(cm200_allows_torque(&cm));

    cm200_note_command_tx(&cm, 2u, false, 0, 350u);
    cm200_update_stale(&cm, 361u);
    EXPECT_TRUE(cm.frame[CM200_FRAME_MOTOR_POSITION].stale);
    EXPECT_FALSE(cm200_allows_torque(&cm));
}

static void test_cm200_counter_echo_fault_and_timer_reset_detection(void)
{
    cm200_t cm;
    uint8_t aa[8] = {0};
    uint8_t ac[8] = {0};
    uint8_t ab[8] = {0};

    cm200_init(&cm);
    feed_cm200_required_frames(&cm, 3u, 100u);
    EXPECT_TRUE(cm200_allows_torque(&cm));

    cm200_note_command_tx(&cm, 5u, true, 500, 120u);
    aa[0] = 5u;
    aa[2] = 1u;
    aa[5] = 0xF0u;
    for(uint32_t i = 0u; i < CM200_INTEGRITY_MISMATCH_LIMIT; i++)
    {
        EXPECT_TRUE(cm200_parse_can_frame(&cm,
                                          CM200_INTERNAL_STATES_CAN_ID,
                                          true,
                                          8u,
                                          aa,
                                          121u + i));
    }
    EXPECT_TRUE(cm.counter_fault);
    EXPECT_TRUE(cm200_has_immediate_fault(&cm));
    EXPECT_FALSE(cm200_allows_torque(&cm));

    aa[5] = 0x60u;
    EXPECT_TRUE(cm200_parse_can_frame(&cm, CM200_INTERNAL_STATES_CAN_ID, true, 8u, aa, 130u));
    EXPECT_FALSE(cm.counter_fault);

    put_u16_le(&ac[0], 123u);
    for(uint32_t i = 0u; i < CM200_INTEGRITY_MISMATCH_LIMIT; i++)
    {
        put_u32_le(&ac[4], 1010u + i);
        EXPECT_TRUE(cm200_parse_can_frame(&cm,
                                          CM200_TORQUE_TIMER_CAN_ID,
                                          true,
                                          8u,
                                          ac,
                                          131u + i));
    }
    EXPECT_TRUE(cm.torque_echo_fault);
    EXPECT_FALSE(cm200_allows_torque(&cm));

    put_u32_le(&ac[4], 900u);
    EXPECT_TRUE(cm200_parse_can_frame(&cm, CM200_TORQUE_TIMER_CAN_ID, true, 8u, ac, 140u));
    EXPECT_TRUE(cm.timer_reset_fault);

    ab[4] = 0x01u;
    EXPECT_TRUE(cm200_parse_can_frame(&cm, CM200_FAULTS_CAN_ID, true, 8u, ab, 141u));
    EXPECT_EQ_U32(cm.run_faults, 1u);
    EXPECT_TRUE(cm200_has_immediate_fault(&cm));
}

static void run_test(const char *name, void (*fn)(void))
{
    int before = failures;
    fn();
    if(failures == before)
    {
        printf("PASS %s\n", name);
    }
}

int main(void)
{
    run_test("segment init clears all fields", test_segment_init_clears_all_fields);
    run_test("ams init clears poisoned state", test_ams_init_clears_poisoned_state);
    run_test("big-endian status packets", test_big_endian_status_packets);
    run_test("all voltage packets cover 75 cells", test_all_voltage_packets_cover_75_cells);
    run_test("all temperature packets cover tails safely", test_all_temperature_packets_cover_tail_safely);
    run_test("all fan packets cover only 10 fans", test_all_fan_packets_cover_only_10_fans);
    run_test("invalid telemetry frames rejected without mutation", test_invalid_telemetry_frames_rejected_without_mutating_payload);
    run_test("CAN frame filtering and bad counts", test_can_frame_filtering_and_bad_counts);
    run_test("estimator frame storage and validation", test_estimator_frame_storage_and_validation);
    run_test("stale state and wraparound", test_stale_state_and_wraparound);
    run_test("full packet sweep counts and last header", test_full_packet_sweep_counts_and_last_header);
    run_test("compact status frame sets torque gate fields", test_compact_status_frame_sets_torque_gate_fields);
    run_test("compact status fault flags block torque", test_compact_status_fault_flags_block_torque);
    run_test("compact electrical thermal and health frames", test_compact_electrical_thermal_and_health_frames);
    run_test("current source diagnostic frame is advisory", test_current_source_diagnostic_frame_is_advisory);
    run_test("logger stream frames decode without torque authority", test_logger_stream_frames_are_decoded_and_advisory);
    run_test("logger snapshot gaps and fragment loss observable", test_logger_snapshot_gap_and_fragment_observability);
    run_test("compact pack voltage matches cell bounds", test_compact_pack_voltage_matches_cell_bounds);
    run_test("compact status drives stale and sequence checks", test_compact_status_drives_stale_and_sequence_checks);
    run_test("compact summary freshness thermal and sanity gates", test_compact_summary_freshness_thermal_and_sanity_gates);
    run_test("compact invalid frames rejected without mutation", test_compact_invalid_frames_rejected_without_mutation);
    run_test("invalid required AMS frame revokes torque immediately", test_invalid_required_ams_frame_revokes_torque_immediately);
    run_test("power authority direction gate", test_power_authority_direction_gate);
    run_test("CM200 broadcast decoding and required gate", test_cm200_broadcast_decoding_and_required_gate);
    run_test("CM200 malformed and freshness fail closed", test_cm200_malformed_and_freshness_fail_closed);
    run_test("CM200 counter echo and timer guards", test_cm200_counter_echo_fault_and_timer_reset_detection);

    if(failures != 0)
    {
        printf("ECU UNIT TESTS FAILED: %d failure(s)\n", failures);
        return 1;
    }

    printf("ALL ECU UNIT TESTS PASSED\n");
    return 0;
}
