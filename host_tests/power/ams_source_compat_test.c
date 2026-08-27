#include "sop/ams_power_can.h"
#include "ext_drivers/ams_power_consumer.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) do { if(!(expression)) { \
    fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #expression); \
    exit(1); } } while(0)

static const uint8_t expected_counter_5[6][8] = {
    { 0x25u, 0x03u, 0x03u, 0x20u, 0x08u, 0x98u, 0xD1u, 0x49u },
    { 0x25u, 0x03u, 0x00u, 0x64u, 0x01u, 0x2Cu, 0x93u, 0xFDu },
    { 0x25u, 0x5Fu, 0x5Au, 0x78u, 0x53u, 0xCBu, 0xD0u, 0x92u },
    { 0x25u, 0x76u, 0x46u, 0x41u, 0x0Bu, 0x0Au, 0x09u, 0xFDu },
    { 0x25u, 0x35u, 0x4Cu, 0x3Eu, 0x01u, 0x08u, 0x58u, 0x1Eu },
    { 0x25u, 0xECu, 0xBAu, 0x87u, 0x02u, 0x34u, 0x21u, 0x68u }
};

static const uint8_t expected_counter_6[6][8] = {
    { 0x26u, 0x03u, 0x03u, 0x20u, 0x08u, 0x98u, 0xD1u, 0xAEu },
    { 0x26u, 0x03u, 0x00u, 0x64u, 0x01u, 0x2Cu, 0x93u, 0x1Au },
    { 0x26u, 0x5Fu, 0x5Au, 0x78u, 0x53u, 0xCBu, 0xD0u, 0x75u },
    { 0x26u, 0x76u, 0x46u, 0x41u, 0x0Bu, 0x0Au, 0x09u, 0x1Au },
    { 0x26u, 0x35u, 0x4Cu, 0x3Eu, 0x01u, 0x08u, 0x58u, 0xF9u },
    { 0x26u, 0xECu, 0xBAu, 0x87u, 0x02u, 0x34u, 0x21u, 0x8Fu }
};

static void fill_snapshot(ams_power_can_snapshot_t *snapshot)
{
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->generation = 1u;
    snapshot->measurement_sequence = 12u;
    snapshot->measurement_timestamp_ms = 1000u;
    snapshot->solve_timestamp_ms = 1050u;
    snapshot->valid = 1u;
    snapshot->authority_valid = 1u;

    snapshot->discharge_current_a[0] = 118.0f;
    snapshot->discharge_current_a[1] = 80.0f;
    snapshot->discharge_current_a[2] = 70.0f;
    snapshot->discharge_current_a[3] = 65.0f;
    snapshot->charge_current_a[0] = -11.0f;
    snapshot->charge_current_a[1] = -10.0f;
    snapshot->charge_current_a[2] = -10.0f;
    snapshot->charge_current_a[3] = -9.0f;
    snapshot->discharge_power_w_1s = 22000.0f;
    snapshot->charge_power_w_1s = 3000.0f;

    snapshot->capacity_soh = 0.95f;
    snapshot->capacity_soh_lower = 0.90f;
    snapshot->resistance_growth_upper = 1.20f;
    snapshot->capacity_confidence_pct = 75u;
    snapshot->resistance_confidence_pct = 80u;
    snapshot->capacity_valid = 1u;
    snapshot->resistance_valid = 1u;

    /* The fixed snapshot intentionally covers all four internal horizons.
     * The 1 s binding/segment appears in DCL/CCL; 0.1/10/30 s metadata appears
     * in the separate bindings frame. */
    snapshot->discharge_binding[0] = 14u;
    snapshot->discharge_binding[1] = 13u;
    snapshot->discharge_binding[2] = 12u;
    snapshot->discharge_binding[3] = 11u;
    snapshot->charge_binding[0] = 10u;
    snapshot->charge_binding[1] = 9u;
    snapshot->charge_binding[2] = 8u;
    snapshot->charge_binding[3] = 7u;

    snapshot->discharge_limiting_segment[0] = 0u;
    snapshot->discharge_limiting_segment[1] = 1u;
    snapshot->discharge_limiting_segment[2] = 2u;
    snapshot->discharge_limiting_segment[3] = 3u;
    snapshot->charge_limiting_segment[0] = 4u;
    snapshot->charge_limiting_segment[1] = 3u;
    snapshot->charge_limiting_segment[2] = 2u;
    snapshot->charge_limiting_segment[3] = 1u;

    snapshot->mission_profile = 1u;
    snapshot->mission_horizon_index = 1u;
    snapshot->thermal_ready = 1u;
    snapshot->fuse_authority_valid = 1u;
    snapshot->limp_latched = 0u;
    snapshot->strategy_reason_flags = 0u;
    snapshot->r0_bootstrap_progress_pct = 88u;
    snapshot->fuse_utilization = 0.76f;
    snapshot->minimum_core_temp_c = 22.0f;
    snapshot->thermal_energy_to_target_wh = 26.4f;
}

static void encode_and_ingest(der26_power_consumer_t *consumer,
                              const ams_power_can_snapshot_t *snapshot,
                              uint8_t counter,
                              uint32_t now_ms,
                              const uint8_t expected[6][8])
{
    uint8_t payload[8];

    ams_power_can_encode_dcl(snapshot, counter, now_ms, payload);
    CHECK(memcmp(payload, expected[0], 8u) == 0);
    CHECK(der26_power_consumer_ingest(consumer, DER26_POWER_DCL_ID,
                                      false, false, 8u, payload, now_ms));
    ams_power_can_encode_ccl(snapshot, counter, now_ms, payload);
    CHECK(memcmp(payload, expected[1], 8u) == 0);
    CHECK(der26_power_consumer_ingest(consumer, DER26_POWER_CCL_ID,
                                      false, false, 8u, payload, now_ms + 1u));
    ams_power_can_encode_soh(snapshot, counter, now_ms, payload);
    CHECK(memcmp(payload, expected[2], 8u) == 0);
    CHECK(der26_power_consumer_ingest(consumer, DER26_POWER_SOH_ID,
                                      false, false, 8u, payload, now_ms + 2u));
    ams_power_can_encode_envelope(snapshot, counter, now_ms, payload);
    CHECK(memcmp(payload, expected[3], 8u) == 0);
    CHECK(der26_power_consumer_ingest(consumer, DER26_POWER_ENVELOPE_ID,
                                      false, false, 8u, payload, now_ms + 3u));
    ams_power_can_encode_strategy(snapshot, counter, now_ms, payload);
    CHECK(memcmp(payload, expected[4], 8u) == 0);
    CHECK(der26_power_consumer_ingest(consumer, DER26_POWER_STRATEGY_ID,
                                      false, false, 8u, payload, now_ms + 4u));
    ams_power_can_encode_bindings(snapshot, counter, now_ms, payload);
    CHECK(memcmp(payload, expected[5], 8u) == 0);
    CHECK(der26_power_consumer_ingest(consumer, DER26_POWER_BINDINGS_ID,
                                      false, false, 8u, payload, now_ms + 5u));
}

int main(void)
{
    ams_power_can_snapshot_t snapshot;
    der26_power_consumer_t consumer;
    der26_power_immediate_authority_t authority;
    der26_power_feasibility_envelope_t envelope;
    der26_power_resource_state_t resource;

    CHECK(AMS_POWER_CAN_PROTOCOL_VERSION == DER26_POWER_PROTOCOL_VERSION);
    CHECK(AMS_POWER_CAN_DCL_ID == DER26_POWER_DCL_ID);
    CHECK(AMS_POWER_CAN_CCL_ID == DER26_POWER_CCL_ID);
    CHECK(AMS_POWER_CAN_SOH_ID == DER26_POWER_SOH_ID);
    CHECK(AMS_POWER_CAN_ENVELOPE_ID == DER26_POWER_ENVELOPE_ID);
    CHECK(AMS_POWER_CAN_STRATEGY_ID == DER26_POWER_STRATEGY_ID);
    CHECK(AMS_POWER_CAN_BINDINGS_ID == DER26_POWER_BINDINGS_ID);

    fill_snapshot(&snapshot);
    der26_power_consumer_init(&consumer);
    encode_and_ingest(&consumer, &snapshot, 5u, 1100u,
                      expected_counter_5);
    CHECK(!der26_power_consumer_get_immediate_authority(&consumer, 1106u,
                                                        &authority));
    encode_and_ingest(&consumer, &snapshot, 6u, 1120u,
                      expected_counter_6);

    CHECK(der26_power_consumer_get_immediate_authority(&consumer, 1126u,
                                                       &authority));
    CHECK(authority.counter == 6u);
    CHECK(authority.discharge.authorized != 0u);
    CHECK(authority.charge_regen.authorized != 0u);
    CHECK(fabsf(authority.discharge.current_limit_a - 80.0f) < 0.001f);
    CHECK(fabsf(authority.charge_regen.current_limit_a - 10.0f) < 0.001f);

    CHECK(der26_power_consumer_get_feasibility_envelope(&consumer, 1126u,
                                                        &envelope));
    CHECK(envelope.binding_metadata_valid != 0u);
    CHECK(fabsf(envelope.discharge_constant_current_feasible_a[0] - 118.0f)
          < 0.001f);
    CHECK(fabsf(envelope.discharge_constant_current_feasible_a[1] - 70.0f)
          < 0.001f);
    CHECK(fabsf(envelope.discharge_constant_current_feasible_a[2] - 65.0f)
          < 0.001f);

    CHECK(der26_power_consumer_get_resource_state(&consumer, 1126u,
                                                  &resource));
    CHECK(resource.counter == 6u);
    CHECK(resource.valid != 0u);
    CHECK(fabsf(resource.fuse_utilization - 0.76f) < 0.001f);

    puts("ams_source_compat_test PASS");
    return 0;
}
