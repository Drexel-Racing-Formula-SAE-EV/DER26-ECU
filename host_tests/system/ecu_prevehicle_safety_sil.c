/*
 * Pre-vehicle AMS/CAN authority SIL.
 *
 * Exercises the production ECU AMS decoder with the production power consumer.
 * The focus is negative authority: passive logger traffic, legacy traffic,
 * malformed frames, stale data, sequence discontinuities and tick wrap may
 * never manufacture or preserve torque authority.
 */
#include "ext_drivers/ams.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ECU_PREVEHICLE_FUZZ_CYCLES
#define ECU_PREVEHICLE_FUZZ_CYCLES 200000u
#endif

#define CHECK(x) do { if(!(x)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); \
} } while(0)

static uint32_t rng_state = 0xD326C0DEu;
static uint32_t rng_next(void)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}

static void put_u16(uint8_t p[8], uint8_t o, uint16_t v)
{
    p[o] = (uint8_t)(v >> 8u);
    p[o + 1u] = (uint8_t)v;
}

static void finish_power(uint16_t id, uint8_t p[8])
{
    p[7] = der26_power_crc8(id, p);
}

static void make_power_bundle(uint8_t counter,
                              uint8_t p[DER26_POWER_CORE_FRAME_COUNT][8],
                              bool authorize)
{
    const uint8_t valid = DER26_POWER_FLAG_VALID |
                          DER26_POWER_FLAG_AUTHORITY_VALID;
    memset(p, 0, DER26_POWER_CORE_FRAME_COUNT * 8u);
    for(uint8_t i = 0u; i < DER26_POWER_CORE_FRAME_COUNT; i++)
    {
        p[i][0] = (uint8_t)((DER26_POWER_PROTOCOL_VERSION << 4u) |
                            (counter & 0x0Fu));
    }
    p[0][1] = authorize ? valid : DER26_POWER_FLAG_FALLBACK;
    p[1][1] = authorize ? valid : DER26_POWER_FLAG_FALLBACK;
    if(authorize)
    {
        put_u16(p[0], 2u, 800u);
        put_u16(p[0], 4u, 2200u);
        p[0][6] = 0xE2u;
        put_u16(p[1], 2u, 100u);
        put_u16(p[1], 4u, 300u);
        p[1][6] = 0xD3u;
        p[3][1] = 118u; p[3][2] = 70u; p[3][3] = 65u;
        p[3][4] = 11u;  p[3][5] = 10u; p[3][6] = 9u;
    }
    p[2][1] = 95u; p[2][2] = 90u; p[2][3] = 120u; p[2][4] = 83u;
    p[2][5] = (uint8_t)(0x80u | 75u);
    p[2][6] = (uint8_t)(0x80u | 80u);
    for(uint8_t i = 0u; i < DER26_POWER_CORE_FRAME_COUNT; i++)
    {
        finish_power((uint16_t)(DER26_POWER_DCL_ID + i), p[i]);
    }
}

static void ingest_power_bundle(ams_t *ams, uint8_t counter,
                                uint32_t now, bool authorize)
{
    uint8_t p[DER26_POWER_CORE_FRAME_COUNT][8];
    make_power_bundle(counter, p, authorize);
    for(uint8_t i = 0u; i < DER26_POWER_CORE_FRAME_COUNT; i++)
    {
        CHECK(ams_parse_can_frame(ams,
                                  (uint16_t)(DER26_POWER_DCL_ID + i),
                                  true, 8u, p[i], now + i));
    }
}

static void feed_compact(ams_t *ams, uint8_t seq, uint32_t now)
{
    const uint8_t status[8] = {
        AMS_ECU_COMPACT_PROTOCOL_VERSION, seq, 1u, 0x71u, 0u, 0u, 0u, 0u
    };
    const uint8_t electrical[8] = {
        0x0Bu, 0xB8u, 0u, 0u, 0x0Bu, 0xB8u, 0x10u, 0x04u
    };
    const uint8_t thermal[8] = {
        0x01u, 0x2Cu, 0x00u, 0xC8u, 0x00u, 0xFAu, 50u, 0u
    };
    const uint8_t health[8] = {0u, 0u, 0u, 0u, 0u, 0u, 75u, 120u};
    CHECK(ams_parse_can_frame(ams, AMS_ECU_STATUS_CANBUS_ID,
                              true, 8u, status, now));
    CHECK(ams_parse_can_frame(ams, AMS_ECU_ELECTRICAL_CANBUS_ID,
                              true, 8u, electrical, now + 1u));
    CHECK(ams_parse_can_frame(ams, AMS_ECU_THERMAL_CANBUS_ID,
                              true, 8u, thermal, now + 2u));
    CHECK(ams_parse_can_frame(ams, AMS_ECU_HEALTH_CANBUS_ID,
                              true, 8u, health, now + 3u));
}

static void establish_authority(ams_t *ams, uint32_t now)
{
    feed_compact(ams, 1u, now);
    ingest_power_bundle(ams, 0u, now + 10u, true);
    ingest_power_bundle(ams, 1u, now + 20u, true);
    ams_update_stale(ams, now + 30u);
    CHECK(ams_allows_torque(ams));
}

typedef struct
{
    bool bms_ok;
    bool bms_inhibited;
    bool hard;
    bool soft;
    bool voltage_valid;
    bool current_valid;
    bool temp_valid;
    bool can_fault;
    bool voltage_fault;
    bool temp_fault;
    bool current_fault;
    bool charger_fault;
    bool adbms_fault;
    bool heartbeat_fault;
    bool compact_protocol_valid;
    bool compact_sequence_fault;
    uint8_t compact_sequence;
    uint32_t last_status;
    uint32_t last_electrical;
    uint32_t last_thermal;
} safety_image_t;

static safety_image_t safety_image(const ams_t *a)
{
    safety_image_t s = {
        a->bms_ok, a->bms_inhibited, a->ams_hard_fault, a->ams_soft_fault,
        a->voltage_valid, a->current_valid, a->temp_valid, a->ams_can_fault,
        a->voltage_fault, a->temp_fault, a->current_fault, a->charger_fault,
        a->adbms_diag_fault, a->task_heartbeat_fault,
        a->compact_protocol_valid, a->compact_sequence_fault,
        a->compact_sequence, a->last_status_rx_tick,
        a->last_electrical_rx_tick, a->last_thermal_rx_tick
    };
    return s;
}

static void test_passive_logger_flood_cannot_keep_authority_alive(void)
{
    ams_t a;
    uint8_t payload[8] = {0};
    ams_init(&a);
    establish_authority(&a, 100u);

    for(uint32_t i = 0u; i < 50000u; i++)
    {
        uint32_t id = AMS_LOGGER_CAN_ID_FIRST +
            (rng_next() % (AMS_LOGGER_CAN_ID_LAST - AMS_LOGGER_CAN_ID_FIRST + 1u));
        for(uint8_t j = 0u; j < 8u; j++) payload[j] = (uint8_t)rng_next();
        (void)ams_parse_can_frame(&a, id, true, 8u, payload, 700u + i);
    }

    /* Passive traffic updates diagnostic last_rx only. Per-frame compact and
     * power freshness must still expire and revoke authority. */
    ams_update_stale(&a, 50000u);
    CHECK(!ams_allows_torque(&a));
    CHECK(a.compact_status_stale);
    CHECK(a.power_authority_stale);
}

static void test_passive_logger_never_mutates_safety_state(void)
{
    ams_t a;
    uint8_t payload[8];
    ams_init(&a);
    establish_authority(&a, 1000u);
    const safety_image_t before = safety_image(&a);

    for(uint32_t i = 0u; i < ECU_PREVEHICLE_FUZZ_CYCLES; i++)
    {
        uint32_t id = AMS_LOGGER_CAN_ID_FIRST +
            (rng_next() % (AMS_LOGGER_CAN_ID_LAST - AMS_LOGGER_CAN_ID_FIRST + 1u));
        for(uint8_t j = 0u; j < 8u; j++) payload[j] = (uint8_t)rng_next();
        (void)ams_parse_can_frame(&a, id, true, 8u, payload, 1020u);
    }
    const safety_image_t after = safety_image(&a);
    CHECK(memcmp(&before, &after, sizeof(before)) == 0);
    CHECK(ams_allows_torque(&a));
}

static void test_fresh_boot_logger_and_legacy_fuzz_never_grants(void)
{
    ams_t a;
    uint8_t payload[8];
    ams_init(&a);

    for(uint32_t i = 0u; i < ECU_PREVEHICLE_FUZZ_CYCLES; i++)
    {
        for(uint8_t j = 0u; j < 8u; j++) payload[j] = (uint8_t)rng_next();
        if((rng_next() & 1u) != 0u)
        {
            uint32_t id = AMS_LOGGER_CAN_ID_FIRST +
                (rng_next() % (AMS_LOGGER_CAN_ID_LAST - AMS_LOGGER_CAN_ID_FIRST + 1u));
            (void)ams_parse_can_frame(&a, id, true, 8u, payload, i);
        }
        else
        {
            (void)ams_parse_can_frame(&a, AMS_TELEM_CANBUS_ID,
                                      true, 8u, payload, i);
        }
        CHECK(!ams_allows_torque(&a));
    }
}

static void test_partial_and_corrupt_power_fail_closed(void)
{
    ams_t a;
    uint8_t p[DER26_POWER_CORE_FRAME_COUNT][8];
    ams_init(&a);
    feed_compact(&a, 1u, 100u);
    make_power_bundle(0u, p, true);
    for(uint8_t i = 0u; i < DER26_POWER_CORE_FRAME_COUNT - 1u; i++)
    {
        CHECK(ams_parse_can_frame(&a, (uint16_t)(DER26_POWER_DCL_ID + i),
                                  true, 8u, p[i], 110u + i));
    }
    CHECK(!ams_allows_torque(&a));

    /* Do not reuse the partially staged counter: the production consumer
     * rejects duplicate/replayed members of an in-progress generation.  Two
     * fresh coherent generations are required before authority is granted. */
    ingest_power_bundle(&a, 1u, 120u, true);
    ingest_power_bundle(&a, 2u, 130u, true);
    CHECK(ams_allows_torque(&a));

    make_power_bundle(3u, p, true);
    p[0][7] ^= 0x01u;
    CHECK(!ams_parse_can_frame(&a, DER26_POWER_DCL_ID,
                               true, 8u, p[0], 140u));
    CHECK(!a.power_authority_valid);
    CHECK(!ams_allows_torque(&a));
}

static void test_sequence_fault_and_recovery(void)
{
    ams_t a;
    uint8_t status[8] = {
        AMS_ECU_COMPACT_PROTOCOL_VERSION, 5u, 1u, 0x71u, 0u, 0u, 0u, 0u
    };
    ams_init(&a);
    establish_authority(&a, 200u);

    status[1] = 9u;
    CHECK(ams_parse_can_frame(&a, AMS_ECU_STATUS_CANBUS_ID,
                              true, 8u, status, 240u));
    CHECK(a.compact_sequence_fault);
    CHECK(!ams_allows_torque(&a));

    status[1] = 10u;
    CHECK(ams_parse_can_frame(&a, AMS_ECU_STATUS_CANBUS_ID,
                              true, 8u, status, 250u));
    CHECK(!a.compact_sequence_fault);
    CHECK(ams_allows_torque(&a));
}

static void test_tick_wrap_staleness(void)
{
    ams_t a;
    const uint32_t base = UINT32_MAX - 200u;
    ams_init(&a);
    feed_compact(&a, 1u, base);
    ingest_power_bundle(&a, 0u, base + 10u, true);
    ingest_power_bundle(&a, 1u, base + 20u, true);

    /* Wrap by 250 ms: compact data remains within its 500 ms bound. */
    ams_update_stale(&a, (uint32_t)(base + 250u));
    CHECK(!a.compact_status_stale);

    /* Wrap by 700 ms: the unsigned subtraction must classify stale. */
    ams_update_stale(&a, (uint32_t)(base + 700u));
    CHECK(a.compact_status_stale);
    CHECK(!ams_allows_torque(&a));
}

static void test_malformed_required_frame_invalidates_immediately(void)
{
    ams_t a;
    uint8_t bad[8] = {0};
    ams_init(&a);
    establish_authority(&a, 300u);
    CHECK(!ams_parse_can_frame(&a, AMS_ECU_ELECTRICAL_CANBUS_ID,
                               true, 7u, bad, 340u));
    ams_invalidate_can_frame(&a, AMS_ECU_ELECTRICAL_CANBUS_ID);
    CHECK(!a.compact_electrical_valid);
    CHECK(!ams_allows_torque(&a));
}

int main(void)
{
    test_passive_logger_flood_cannot_keep_authority_alive();
    puts("PASS passive logger flood cannot keep torque authority alive");
    test_passive_logger_never_mutates_safety_state();
    puts("PASS passive logger fuzz is safety-state noninterfering");
    test_fresh_boot_logger_and_legacy_fuzz_never_grants();
    puts("PASS logger/legacy fuzz cannot manufacture authority from boot");
    test_partial_and_corrupt_power_fail_closed();
    puts("PASS partial/corrupt power authority fails closed");
    test_sequence_fault_and_recovery();
    puts("PASS compact sequence discontinuity blocks until coherent recovery");
    test_tick_wrap_staleness();
    puts("PASS stale-time arithmetic is safe across uint32 tick wrap");
    test_malformed_required_frame_invalidates_immediately();
    puts("PASS malformed required compact frame revokes authority");
    printf("ecu_prevehicle_safety_sil PASS (%u passive fuzz cycles per stress case)\n",
           (unsigned)ECU_PREVEHICLE_FUZZ_CYCLES);
    return 0;
}
