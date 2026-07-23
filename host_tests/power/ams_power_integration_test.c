#include "ext_drivers/ams.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x); exit(1);} } while(0)

static void put_u16(uint8_t p[8], uint8_t o, uint16_t v)
{
    p[o] = (uint8_t)(v >> 8u);
    p[o + 1u] = (uint8_t)v;
}

static void finish(uint16_t id, uint8_t p[8])
{
    p[7] = der26_power_crc8(id, p);
}

static void make_bundle(uint8_t counter,
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
        p[3][4] = 11u; p[3][5] = 10u; p[3][6] = 9u;
    }
    p[2][1] = 95u; p[2][2] = 90u; p[2][3] = 120u; p[2][4] = 83u;
    p[2][5] = (uint8_t)(0x80u | 75u);
    p[2][6] = (uint8_t)(0x80u | 80u);
    for(uint8_t i = 0u; i < DER26_POWER_CORE_FRAME_COUNT; i++)
    {
        finish((uint16_t)(DER26_POWER_DCL_ID + i), p[i]);
    }
}

static void ingest_bundle(ams_t *ams, uint8_t counter, uint32_t now, bool authorize)
{
    uint8_t p[DER26_POWER_CORE_FRAME_COUNT][8];
    make_bundle(counter, p, authorize);
    for(uint8_t i = 0u; i < DER26_POWER_CORE_FRAME_COUNT; i++)
    {
        CHECK(ams_parse_can_frame(ams, (uint16_t)(DER26_POWER_DCL_ID + i),
                                  true, 8u, p[i], now + i));
    }
}

static void feed_compact(ams_t *ams, uint8_t seq, uint32_t now)
{
    uint8_t status[8] = {AMS_ECU_COMPACT_PROTOCOL_VERSION, seq, 1u, 0x71u, 0u, 0u, 0u, 0u};
    uint8_t electrical[8] = {0x0Bu,0xB8u,0u,0u,0x0Bu,0xB8u,0x10u,0x04u};
    uint8_t thermal[8] = {0x01u,0x2Cu,0x00u,0xC8u,0x00u,0xFAu,50u,0u};
    CHECK(ams_parse_can_frame(ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, status, now));
    CHECK(ams_parse_can_frame(ams, AMS_ECU_ELECTRICAL_CANBUS_ID, true, 8u, electrical, now));
    CHECK(ams_parse_can_frame(ams, AMS_ECU_THERMAL_CANBUS_ID, true, 8u, thermal, now));
}

int main(void)
{
    ams_t ams;
    der26_power_immediate_authority_t authority;
    der26_power_feasibility_envelope_t envelope;
    uint8_t mission[8];
    uint8_t bad[8] = {0};

    ams_init(&ams);
    feed_compact(&ams, 1u, 10u);
    ams_update_stale(&ams, 10u);
    CHECK(!ams_allows_torque(&ams));

    ingest_bundle(&ams, 0u, 20u, true);
    ingest_bundle(&ams, 1u, 30u, true);
    ams_update_stale(&ams, 35u);
    CHECK(ams_allows_torque(&ams));
    CHECK(ams_get_immediate_power_authority(&ams, 35u, &authority));
    CHECK(authority.discharge.authorized != 0u);
    CHECK(authority.discharge.current_limit_a == 80.0f);

    /* Compact electrical data remains an independent health gate.  A pack
     * voltage that cannot fit between 75 * min_cell and 75 * max_cell must
     * block torque even while the protocol-v2 scalar bundle is valid. */
    {
        uint8_t impossible_electrical[8] =
            {0u, 0u, 0u, 0u, 0x0Bu, 0xB8u, 0x10u, 0x04u};
        CHECK(ams_parse_can_frame(&ams, AMS_ECU_ELECTRICAL_CANBUS_ID, true,
                                  8u, impossible_electrical, 35u));
        ams_update_stale(&ams, 35u);
        CHECK(!ams.compact_electrical_sane);
        CHECK(!ams_allows_torque(&ams));
        feed_compact(&ams, 2u, 35u);
        ams_update_stale(&ams, 35u);
        CHECK(ams_allows_torque(&ams));
    }

    /* Base AMS health must remain usable when only charge/regen authority is
     * present; the final command-direction gate then rejects positive torque
     * and accepts negative torque. */
    {
        der26_power_immediate_authority_t saved = ams.power_authority;
        ams.power_authority.discharge.authorized = 0u;
        ams.power_authority.discharge.current_limit_a = 0.0f;
        ams.power_authority.discharge.power_limit_w = 0.0f;
        CHECK(ams_allows_torque(&ams));
        CHECK(!ams_power_authority_allows_torque_command(&ams.power_authority, 100));
        CHECK(ams_power_authority_allows_torque_command(&ams.power_authority, -100));
        ams.power_authority = saved;
    }

    CHECK(ams_get_feasibility_envelope(&ams, 35u, &envelope));
    CHECK(DER26_POWER_WIRE_HORIZON_COUNT == 3u);
    CHECK(envelope.discharge_constant_current_feasible_a[0] == 118.0f);
    CHECK(envelope.discharge_constant_current_feasible_a[1] == 70.0f);
    CHECK(envelope.discharge_constant_current_feasible_a[2] == 65.0f);

    /* Optional advisory damage cannot revoke scalar authority, including the
     * malformed-DLC path that previously cleared the cached scalar bundle. */
    CHECK(!ams_parse_can_frame(&ams, DER26_POWER_STRATEGY_ID, true, 8u, bad, 36u));
    CHECK(ams.power_authority_valid);
    CHECK(!ams_parse_can_frame(&ams, DER26_POWER_BINDINGS_ID, true, 7u, bad, 36u));
    CHECK(ams.power_authority_valid);
    CHECK(ams_get_immediate_power_authority(&ams, 36u, &authority));
    ams_update_stale(&ams, 36u);
    CHECK(ams_allows_torque(&ams));

    /* A malformed required power frame revokes authority immediately. */
    CHECK(!ams_parse_can_frame(&ams, DER26_POWER_DCL_ID, true, 7u, bad, 37u));
    ams_update_stale(&ams, 37u);
    CHECK(!ams_allows_torque(&ams));
    CHECK(!ams_get_immediate_power_authority(&ams, 37u, &authority));

    ingest_bundle(&ams, 2u, 40u, true);
    ingest_bundle(&ams, 3u, 50u, true);
    ams_update_stale(&ams, 55u);
    CHECK(ams_allows_torque(&ams));

    /* A CRC-bad required frame must clear the task-facing cache in the receive
     * path, without waiting for ams_update_stale()/the error task. */
    {
        uint8_t corrupt[DER26_POWER_CORE_FRAME_COUNT][8];
        make_bundle(4u, corrupt, true);
        corrupt[0][7] ^= 0x01u;
        CHECK(!ams_parse_can_frame(&ams, DER26_POWER_DCL_ID, true, 8u, corrupt[0], 56u));
        CHECK(!ams.power_authority_valid);
        CHECK(ams.power_authority_stale);
        CHECK(ams.power_authority.discharge.current_limit_a == 0.0f);
    }

    ingest_bundle(&ams, 4u, 57u, true);
    ingest_bundle(&ams, 5u, 58u, true);
    CHECK(ams.power_authority_valid);

    /* A newer coherent zero bundle overrides the older positive authority. */
    ingest_bundle(&ams, 6u, 60u, false);
    ams_update_stale(&ams, 65u);
    CHECK(!ams_allows_torque(&ams));
    CHECK(ams_get_immediate_power_authority(&ams, 65u, &authority));
    CHECK(authority.discharge.authorized == 0u);
    CHECK(authority.discharge.current_limit_a == 0.0f);

    CHECK(ams_encode_mission_request(DER26_MISSION_QUALIFY, 5u, true, mission));
    CHECK(mission[0] == 0x15u);
    CHECK(mission[7] == der26_power_crc8(DER26_MISSION_REQUEST_ID, mission));

    ams_update_stale(&ams, 400u);
    CHECK(!ams_get_immediate_power_authority(&ams, 400u, &authority));
    CHECK(!ams_allows_torque(&ams));

    puts("ams_power_integration_test PASS");
    return 0;
}
