#include "ext_drivers/ams.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define EXPECT_TRUE(expr) do { if(!(expr)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); failures++; } } while(0)
#define EXPECT_FALSE(expr) EXPECT_TRUE(!(expr))
#define EXPECT_EQ_U16(actual, expected) do { uint16_t a=(uint16_t)(actual); uint16_t e=(uint16_t)(expected); if(a != e) { printf("FAIL %s:%d: %s=%u expected=%u\n", __FILE__, __LINE__, #actual, (unsigned)a, (unsigned)e); failures++; } } while(0)
#define EXPECT_EQ_I16(actual, expected) do { int16_t a=(int16_t)(actual); int16_t e=(int16_t)(expected); if(a != e) { printf("FAIL %s:%d: %s=%d expected=%d\n", __FILE__, __LINE__, #actual, (int)a, (int)e); failures++; } } while(0)
#define EXPECT_EQ_U8(actual, expected) do { uint8_t a=(uint8_t)(actual); uint8_t e=(uint8_t)(expected); if(a != e) { printf("FAIL %s:%d: %s=%u expected=%u\n", __FILE__, __LINE__, #actual, (unsigned)a, (unsigned)e); failures++; } } while(0)
#define EXPECT_EQ_U32(actual, expected) do { uint32_t a=(uint32_t)(actual); uint32_t e=(uint32_t)(expected); if(a != e) { printf("FAIL %s:%d: %s=%lu expected=%lu\n", __FILE__, __LINE__, #actual, (unsigned long)a, (unsigned long)e); failures++; } } while(0)

static void put_u16_be(uint8_t *dst, uint16_t v)
{
    dst[0] = (uint8_t)(v >> 8u);
    dst[1] = (uint8_t)(v & 0xFFu);
}

static void make_packet(uint8_t frame[8], uint16_t header, uint16_t d0, uint16_t d1, uint16_t d2)
{
    put_u16_be(&frame[0], header);
    put_u16_be(&frame[2], d0);
    put_u16_be(&frame[4], d1);
    put_u16_be(&frame[6], d2);
}

static void test_init_and_75s_layout(void)
{
    ams_t ams;
    ams_init(&ams);

    EXPECT_EQ_U16(NSEGS, 5u);
    EXPECT_EQ_U16(NVOLTS, 15u);
    EXPECT_EQ_U16(NTEMPS, 17u);
    EXPECT_TRUE(ams.stale == false);
    EXPECT_EQ_U32(ams.rx_count, 0u);
}

static void test_status_packet_parse(void)
{
    ams_t ams;
    uint8_t frame[8];
    ams_init(&ams);
    make_packet(frame, 0u, 1u, 2u, 300u);

    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_TELEM_CANBUS_ID, true, 8u, frame, 10u));
    EXPECT_EQ_U16(ams.state, 1u);
    EXPECT_EQ_U16(ams.air_state, 2u);
    EXPECT_EQ_U16(ams.current, 300u);
    EXPECT_EQ_U16(ams.last_packet_header, 0u);
    EXPECT_EQ_U32(ams.last_rx_tick, 10u);
    EXPECT_EQ_U32(ams.rx_count, 1u);
}

static void test_75th_cell_packet_parse(void)
{
    ams_t ams;
    uint8_t frame[8];
    ams_init(&ams);

    make_packet(frame, 27u, 730u, 740u, 750u);
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_TELEM_CANBUS_ID, true, 8u, frame, 20u));

    EXPECT_EQ_U16(ams.segs[4].volts[12], 730u);
    EXPECT_EQ_U16(ams.segs[4].volts[13], 740u);
    EXPECT_EQ_U16(ams.segs[4].volts[14], 750u);
}

static void test_temp_tail_and_fan_tail_parse(void)
{
    ams_t ams;
    uint8_t frame[8];
    ams_init(&ams);

    make_packet(frame, 57u, 151u, 161u, 999u);
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_TELEM_CANBUS_ID, true, 8u, frame, 30u));
    EXPECT_EQ_U16(ams.segs[4].temps[15], 151u);
    EXPECT_EQ_U16(ams.segs[4].temps[16], 161u);

    make_packet(frame, 61u, 90u, 999u, 999u);
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_TELEM_CANBUS_ID, true, 8u, frame, 40u));
    EXPECT_EQ_U16(ams.fans[9], 90u);
}

static void test_invalid_frames_rejected(void)
{
    ams_t ams;
    uint8_t frame[8];
    ams_init(&ams);

    make_packet(frame, 62u, 1u, 2u, 3u);
    EXPECT_FALSE(ams_parse_can_frame(&ams, AMS_TELEM_CANBUS_ID, true, 8u, frame, 1u));
    EXPECT_EQ_U32(ams.bad_rx_count, 1u);

    make_packet(frame, 0u, 1u, 2u, 3u);
    EXPECT_FALSE(ams_parse_can_frame(&ams, AMS_TELEM_CANBUS_ID, true, 7u, frame, 2u));
    EXPECT_EQ_U32(ams.bad_rx_count, 2u);

    EXPECT_FALSE(ams_parse_can_frame(&ams, AMS_TELEM_CANBUS_ID, false, 8u, frame, 3u));
    EXPECT_EQ_U32(ams.bad_rx_count, 3u);

    EXPECT_FALSE(ams_parse_can_frame(&ams, 0x123u, true, 8u, frame, 4u));
    EXPECT_EQ_U32(ams.bad_rx_count, 3u);
}

static void test_stale_logic(void)
{
    ams_t ams;
    uint8_t frame[8];
    ams_init(&ams);

    ams_update_stale(&ams, 100u);
    EXPECT_TRUE(ams.stale);

    make_packet(frame, 0u, 1u, 2u, 3u);
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_TELEM_CANBUS_ID, true, 8u, frame, 1000u));
    EXPECT_FALSE(ams.stale);

    ams_update_stale(&ams, 1200u);
    EXPECT_FALSE(ams.stale);
    ams_update_stale(&ams, 1601u);
    EXPECT_TRUE(ams.stale);
}

static void test_estimator_packet_raw_storage(void)
{
    ams_t ams;
    uint8_t frame[8];
    ams_init(&ams);

    make_packet(frame, 0x1111u, 0x2222u, 0x3333u, 0x4444u);
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ESTIMATOR_CANBUS_ID, true, 8u, frame, 77u));
    EXPECT_TRUE(ams.estimator.valid);
    EXPECT_EQ_U16(ams.estimator.words[0], 0x1111u);
    EXPECT_EQ_U16(ams.estimator.words[1], 0x2222u);
    EXPECT_EQ_U16(ams.estimator.words[2], 0x3333u);
    EXPECT_EQ_U16(ams.estimator.words[3], 0x4444u);
    EXPECT_EQ_U32(ams.estimator.last_rx_tick, 77u);
    EXPECT_EQ_U32(ams.estimator.rx_count, 1u);
}


static void test_compact_status_and_electrical_parse(void)
{
    ams_t ams;
    uint8_t status[8] = {AMS_ECU_COMPACT_PROTOCOL_VERSION, 1u, 2u, 0x71u, 0u, 0u, 0u, 0u};
    uint8_t electrical[8] = {0x0Cu, 0x80u, 0x00u, 0x7Bu, 0x0Bu, 0xEAu, 0x10u, 0x68u};
    uint8_t thermal[8] = {0x01u, 0x2Cu, 0x00u, 0xC8u, 0x00u, 0xFAu, 50u, 0u};
    ams_init(&ams);

    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, status, 100u));
    EXPECT_TRUE(ams.compact_status_valid);
    EXPECT_FALSE(ams_allows_torque(&ams));
    EXPECT_EQ_U8(ams.compact_sequence, 1u);
    EXPECT_EQ_U8(ams.compact_state, 2u);

    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_ELECTRICAL_CANBUS_ID, true, 8u, electrical, 110u));
    EXPECT_TRUE(ams.compact_electrical_valid);
    EXPECT_EQ_U16(ams.pack_voltage_0p1v, 3200u);
    EXPECT_EQ_I16(ams.pack_current_0p1a, 123);
    EXPECT_EQ_U16(ams.min_cell_mv, 3050u);
    EXPECT_EQ_U16(ams.max_cell_mv, 4200u);
    EXPECT_FALSE(ams_allows_torque(&ams));

    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_THERMAL_CANBUS_ID, true, 8u, thermal, 120u));
    EXPECT_TRUE(ams_allows_torque(&ams));
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
    run_test("init and 75s layout", test_init_and_75s_layout);
    run_test("status packet parse", test_status_packet_parse);
    run_test("75th cell packet parse", test_75th_cell_packet_parse);
    run_test("temperature and fan tail parse", test_temp_tail_and_fan_tail_parse);
    run_test("invalid frames rejected", test_invalid_frames_rejected);
    run_test("stale logic", test_stale_logic);
    run_test("estimator packet raw storage", test_estimator_packet_raw_storage);
    run_test("compact status and electrical parse", test_compact_status_and_electrical_parse);

    if(failures != 0)
    {
        printf("ECU HOST TESTS FAILED: %d failure(s)\n", failures);
        return 1;
    }

    printf("ALL ECU HOST TESTS PASSED\n");
    return 0;
}
