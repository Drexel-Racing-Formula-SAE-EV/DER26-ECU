#include "ext_drivers/ams.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define FAIL_MSG(msg) do { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg)); failures++; } while(0)
#define EXPECT_TRUE(expr) do { if(!(expr)) { printf("FAIL %s:%d: expected true: %s\n", __FILE__, __LINE__, #expr); failures++; } } while(0)
#define EXPECT_FALSE(expr) EXPECT_TRUE(!(expr))
#define EXPECT_EQ_U16(actual, expected) do { uint16_t a_=(uint16_t)(actual); uint16_t e_=(uint16_t)(expected); if(a_ != e_) { printf("FAIL %s:%d: %s=%u expected=%u\n", __FILE__, __LINE__, #actual, (unsigned)a_, (unsigned)e_); failures++; } } while(0)
#define EXPECT_EQ_U32(actual, expected) do { uint32_t a_=(uint32_t)(actual); uint32_t e_=(uint32_t)(expected); if(a_ != e_) { printf("FAIL %s:%d: %s=%lu expected=%lu\n", __FILE__, __LINE__, #actual, (unsigned long)a_, (unsigned long)e_); failures++; } } while(0)
#define EXPECT_EQ_BOOL(actual, expected) do { bool a_=(bool)(actual); bool e_=(bool)(expected); if(a_ != e_) { printf("FAIL %s:%d: %s=%u expected=%u\n", __FILE__, __LINE__, #actual, (unsigned)a_, (unsigned)e_); failures++; } } while(0)

static void put_u16_be(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value >> 8u);
    dst[1] = (uint8_t)(value & 0xFFu);
}

static void make_packet(uint8_t frame[8], uint16_t header, uint16_t d0, uint16_t d1, uint16_t d2)
{
    put_u16_be(&frame[0], header);
    put_u16_be(&frame[2], d0);
    put_u16_be(&frame[4], d1);
    put_u16_be(&frame[6], d2);
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
    EXPECT_EQ_U16(NTEMPS, 17u);
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

    for(uint16_t header = 28u; header <= 57u; header++)
    {
        uint16_t offset = (uint16_t)(header - 28u);
        uint16_t seg = (uint16_t)(offset / 6u);
        uint16_t group = (uint16_t)(offset % 6u);
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

    EXPECT_EQ_U32(ams.rx_count, 30u);
    EXPECT_EQ_U16(ams.last_packet_header, 57u);
}

static void test_all_fan_packets_cover_only_10_fans(void)
{
    ams_t ams;
    uint8_t frame[8];
    ams_init(&ams);

    for(uint16_t header = 58u; header <= 61u; header++)
    {
        uint16_t base = (uint16_t)((header - 58u) * 3u);
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
    EXPECT_EQ_U16(ams.last_packet_header, 61u);
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

    if(failures != 0)
    {
        printf("ECU UNIT TESTS FAILED: %d failure(s)\n", failures);
        return 1;
    }

    printf("ALL ECU UNIT TESTS PASSED\n");
    return 0;
}
