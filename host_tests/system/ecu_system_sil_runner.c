/*
 * ecu_system_sil_runner.c
 *
 * ECU host-side system SIL/fault-injection tests. This is intentionally pure
 * C desktop logic. It does not prove pin wiring, ADC calibration, or real CAN
 * timing; it stress-tests the safety logic that gates RTD and CM200 torque.
 */

#include "ext_drivers/ams.h"
#include "ext_drivers/cm200.h"
#include "ext_drivers/ecu_safety.h"
#include "ecu_config.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef ECU_HOST_LONG_FUZZ_CYCLES
#define ECU_HOST_LONG_FUZZ_CYCLES 10000u
#endif

static int failures = 0;

#define EXPECT_TRUE(expr) do { if(!(expr)) { printf("FAIL %s:%d: expected true: %s\n", __FILE__, __LINE__, #expr); failures++; } } while(0)
#define EXPECT_FALSE(expr) EXPECT_TRUE(!(expr))
#define EXPECT_EQ_U8(actual, expected) do { uint8_t a_=(uint8_t)(actual); uint8_t e_=(uint8_t)(expected); if(a_ != e_) { printf("FAIL %s:%d: %s=%u expected=%u\n", __FILE__, __LINE__, #actual, (unsigned)a_, (unsigned)e_); failures++; } } while(0)
#define EXPECT_EQ_U32(actual, expected) do { uint32_t a_=(uint32_t)(actual); uint32_t e_=(uint32_t)(expected); if(a_ != e_) { printf("FAIL %s:%d: %s=%lu expected=%lu\n", __FILE__, __LINE__, #actual, (unsigned long)a_, (unsigned long)e_); failures++; } } while(0)
#define EXPECT_MEM_EQ(a, b, n) do { if(memcmp((a), (b), (n)) != 0) { printf("FAIL %s:%d: memory mismatch: %s vs %s\n", __FILE__, __LINE__, #a, #b); failures++; } } while(0)

static uint32_t rng_state = 0x26EC075u;

static uint32_t rng_next(void)
{
    rng_state = (rng_state * 1664525u) + 1013904223u;
    return rng_state;
}

static void status_frame(uint8_t frame[8], uint8_t seq, uint8_t status_flags, uint8_t fault_flags)
{
    frame[0] = AMS_ECU_COMPACT_PROTOCOL_VERSION;
    frame[1] = seq;
    frame[2] = 2u;
    frame[3] = status_flags;
    frame[4] = fault_flags;
    frame[5] = 0u;
    frame[6] = 0u;
    frame[7] = 0u;
}

static bool parse_good_summaries(ams_t *ams, uint32_t now_ms)
{
    const uint8_t electrical[8] = {0x0Cu, 0x80u, 0u, 0u, 0x0Bu, 0xB8u, 0x10u, 0x04u};
    const uint8_t thermal[8] = {0x01u, 0x2Cu, 0x00u, 0xC8u, 0x00u, 0xFAu, 50u, 0u};
    return (ams_parse_can_frame(ams, AMS_ECU_ELECTRICAL_CANBUS_ID, true, 8u, electrical, now_ms) &&
            ams_parse_can_frame(ams, AMS_ECU_THERMAL_CANBUS_ID, true, 8u, thermal, now_ms));
}

static bool parse_status(ams_t *ams, uint8_t seq, uint8_t status_flags, uint8_t fault_flags, uint32_t now_ms)
{
    uint8_t frame[8];
    status_frame(frame, seq, status_flags, fault_flags);
    if(!ams_parse_can_frame(ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, frame, now_ms))
    {
        return false;
    }
    return parse_good_summaries(ams, now_ms);
}

static void put_u16_be(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value >> 8u);
    dst[1] = (uint8_t)(value & 0xFFu);
}

static void make_legacy_packet(uint8_t frame[8], uint16_t header, uint16_t d0, uint16_t d1, uint16_t d2)
{
    put_u16_be(&frame[0], header);
    put_u16_be(&frame[2], d0);
    put_u16_be(&frame[4], d1);
    put_u16_be(&frame[6], d2);
}

static ecu_rtd_inputs_t rtd_good_inputs(void)
{
    ecu_rtd_inputs_t in;
    memset(&in, 0, sizeof(in));
    in.tsal = true;
    in.rtd_button = true;
    in.cascadia_ok = true;
    in.brakelight = true;
    return in;
}

static ecu_torque_inputs_t torque_good_inputs(void)
{
    ecu_torque_inputs_t in;
    memset(&in, 0, sizeof(in));
    in.cascadia_ok = true;
    in.rtd_mode = RTD_ENABLED;
    return in;
}

static void test_ams_torque_gate_requires_compact_status(void)
{
    ams_t ams;
    uint8_t legacy[8];
    ams_init(&ams);

    EXPECT_FALSE(ams_allows_torque(&ams));
    make_legacy_packet(legacy, 0u, 1u, 0u, 100u);
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_TELEM_CANBUS_ID, true, 8u, legacy, 10u));
    ams_update_stale(&ams, 20u);
    EXPECT_FALSE(ams_allows_torque(&ams));

    EXPECT_TRUE(parse_status(&ams, 1u, 0x71u, 0x00u, 30u));
    EXPECT_TRUE(ams_allows_torque(&ams));
}

static void test_ams_fault_matrix_blocks_torque(void)
{
    static const uint8_t status_block_bits[] = {
        0x01u, /* BMS_OK absent */
        0x02u, /* BMS inhibited */
        0x04u, /* AMS hard fault */
        0x08u, /* AMS soft fault */
        0x10u, /* voltage valid absent */
        0x20u, /* current valid absent */
        0x40u, /* temp valid absent */
        0x80u, /* AMS CAN fault */
    };
    static const uint8_t fault_block_bits[] = {
        0x01u, /* voltage fault */
        0x02u, /* temp fault */
        0x04u, /* current fault */
        0x10u, /* charger fault */
        0x20u, /* ADBMS diagnostic fault */
        0x40u, /* task heartbeat fault */
        0x80u, /* logger heartbeat fault */
    };
    ams_t ams;
    uint8_t seq = 1u;

    ams_init(&ams);
    EXPECT_TRUE(parse_status(&ams, seq++, 0x71u, 0x00u, 10u));
    EXPECT_TRUE(ams_allows_torque(&ams));

    for(size_t i = 0u; i < sizeof(status_block_bits); i++)
    {
        uint8_t status = 0x71u;
        if(status_block_bits[i] == 0x01u)
        {
            status = (uint8_t)(status & (uint8_t)~0x01u);
        }
        else if((status_block_bits[i] == 0x10u) || (status_block_bits[i] == 0x20u) || (status_block_bits[i] == 0x40u))
        {
            status = (uint8_t)(status & (uint8_t)~status_block_bits[i]);
        }
        else
        {
            status = (uint8_t)(status | status_block_bits[i]);
        }
        EXPECT_TRUE(parse_status(&ams, seq++, status, 0x00u, (uint32_t)(20u + i)));
        EXPECT_FALSE(ams_allows_torque(&ams));
    }

    EXPECT_TRUE(parse_status(&ams, seq++, 0x71u, 0x00u, 100u));
    EXPECT_TRUE(ams_allows_torque(&ams));

    for(size_t i = 0u; i < sizeof(fault_block_bits); i++)
    {
        EXPECT_TRUE(parse_status(&ams, seq++, 0x71u, fault_block_bits[i], (uint32_t)(120u + i)));
        EXPECT_FALSE(ams_allows_torque(&ams));
    }

    /* Reserved IMD bit must stay ignored until firmware actually decodes IMD. */
    EXPECT_TRUE(parse_status(&ams, seq++, 0x71u, 0x08u, 200u));
    EXPECT_TRUE(ams_allows_torque(&ams));
}

static void test_ams_protocol_and_sequence_guards(void)
{
    ams_t ams;
    uint8_t frame[8];
    ams_init(&ams);

    status_frame(frame, 10u, 0x71u, 0x00u);
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, frame, 1000u));
    EXPECT_TRUE(parse_good_summaries(&ams, 1000u));
    EXPECT_TRUE(ams_allows_torque(&ams));

    frame[0] = (uint8_t)(AMS_ECU_COMPACT_PROTOCOL_VERSION + 1u);
    frame[1] = 11u;
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, frame, 1010u));
    EXPECT_FALSE(ams.compact_protocol_valid);
    EXPECT_FALSE(ams_allows_torque(&ams));

    frame[0] = AMS_ECU_COMPACT_PROTOCOL_VERSION;
    frame[1] = 12u;
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, frame, 1020u));
    EXPECT_TRUE(ams.compact_protocol_valid);
    EXPECT_FALSE(ams.compact_sequence_fault);
    EXPECT_TRUE(ams_allows_torque(&ams));

    /* Repeated/stuck status frame must block torque. */
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, frame, 1030u));
    EXPECT_TRUE(ams.compact_sequence_repeated);
    EXPECT_TRUE(ams.compact_sequence_fault);
    EXPECT_FALSE(ams_allows_torque(&ams));

    /* A jump/missed sequence also blocks until the next coherent frame. */
    frame[1] = 15u;
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, frame, 1040u));
    EXPECT_TRUE(ams.compact_sequence_fault);
    EXPECT_FALSE(ams_allows_torque(&ams));
    frame[1] = 16u;
    EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, frame, 1050u));
    EXPECT_TRUE(parse_good_summaries(&ams, 1050u));
    EXPECT_FALSE(ams.compact_sequence_fault);
    EXPECT_TRUE(ams_allows_torque(&ams));

    ams_update_stale(&ams, 1550u);
    EXPECT_FALSE(ams.stale);
    ams_update_stale(&ams, 1551u);
    EXPECT_TRUE(ams.stale);
    EXPECT_FALSE(ams_allows_torque(&ams));
}

static void test_ams_sequence_fuzz_stays_fail_closed(void)
{
    ams_t ams;
    uint8_t frame[8];
    uint8_t expected_seq = 0u;
    uint32_t good_count = 0u;
    uint32_t blocked_count = 0u;

    ams_init(&ams);

    for(uint32_t i = 0u; i < ECU_HOST_LONG_FUZZ_CYCLES; i++)
    {
        uint32_t r = rng_next();
        uint8_t seq;
        uint8_t status = 0x71u;
        uint8_t fault = 0x00u;

        if(i == 0u)
        {
            seq = 1u;
            expected_seq = 2u;
        }
        else if((r & 0x0Fu) == 0u)
        {
            seq = (uint8_t)(expected_seq + 3u); /* sequence jump */
            expected_seq = (uint8_t)(seq + 1u);
        }
        else if((r & 0x1Fu) == 1u)
        {
            seq = (uint8_t)(expected_seq - 1u); /* repeat/stale */
        }
        else
        {
            seq = expected_seq;
            expected_seq = (uint8_t)(expected_seq + 1u);
        }

        if((r & 0x100u) != 0u)
        {
            fault |= 0x20u; /* ADBMS diagnostic fault. */
        }
        if((r & 0x200u) != 0u)
        {
            status &= (uint8_t)~0x20u; /* current invalid. */
        }

        status_frame(frame, seq, status, fault);
        EXPECT_TRUE(ams_parse_can_frame(&ams, AMS_ECU_STATUS_CANBUS_ID, true, 8u, frame, i));
        EXPECT_TRUE(parse_good_summaries(&ams, i));

        bool expected_allow = (!ams.stale &&
                               ams.bms_ok &&
                               ams.voltage_valid &&
                               ams.current_valid &&
                               ams.temp_valid &&
                               !ams.adbms_diag_fault &&
                               ams.compact_protocol_valid &&
                               !ams.compact_sequence_fault);
        if(expected_allow)
        {
            good_count++;
            EXPECT_TRUE(ams_allows_torque(&ams));
        }
        else
        {
            blocked_count++;
            EXPECT_FALSE(ams_allows_torque(&ams));
        }
    }

    EXPECT_TRUE(good_count > 0u);
    EXPECT_TRUE(blocked_count > 0u);
}

static void test_rtd_state_machine_fault_injection(void)
{
    ecu_rtd_inputs_t in = rtd_good_inputs();
    ecu_rtd_step_t step;
    rtd_state_t state = RTD_AWAIT_TSAL;
    uint32_t buzz = 0u;

    in.tsal = false;
    step = ecu_rtd_step(state, buzz, &in, 0u);
    EXPECT_EQ_U8(step.state, RTD_AWAIT_TSAL);
    EXPECT_FALSE(step.buzzer_on);

    in.tsal = true;
    in.rtd_button = true;
    step = ecu_rtd_step(step.state, step.buzz_start_tick, &in, 10u);
    EXPECT_EQ_U8(step.state, RTD_AWAIT_BUTTON_FALSE);

    /* Button must be released once before the press can arm RTD. */
    step = ecu_rtd_step(step.state, step.buzz_start_tick, &in, 20u);
    EXPECT_EQ_U8(step.state, RTD_AWAIT_BUTTON_FALSE);
    in.rtd_button = false;
    step = ecu_rtd_step(step.state, step.buzz_start_tick, &in, 30u);
    EXPECT_EQ_U8(step.state, RTD_AWAIT_CONDITIONS);

    in.rtd_button = true;
    step = ecu_rtd_step(step.state, step.buzz_start_tick, &in, 40u);
    EXPECT_EQ_U8(step.state, RTD_BUZZING);
    EXPECT_TRUE(step.buzzer_on);
    buzz = step.buzz_start_tick;

    step = ecu_rtd_step(step.state, buzz, &in, buzz + ECU_RTD_BUZZ_TIME_MS - 1u);
    EXPECT_EQ_U8(step.state, RTD_BUZZING);
    EXPECT_TRUE(step.buzzer_on);

    /* The dedicated RTD control is momentary; releasing it must not cancel the
     * sound or later drop Ready-to-Drive mode. */
    in.rtd_button = false;
    step = ecu_rtd_step(step.state, buzz, &in, buzz + 500u);
    EXPECT_EQ_U8(step.state, RTD_BUZZING);

    in.faults.ams_fault = true;
    step = ecu_rtd_step(step.state, buzz, &in, buzz + 1000u);
    EXPECT_EQ_U8(step.state, RTD_AWAIT_BUTTON_FALSE);
    EXPECT_FALSE(step.buzzer_on);
    EXPECT_FALSE(step.trip_pulse_requested);

    in.faults.ams_fault = false;
    step = ecu_rtd_step(step.state, step.buzz_start_tick, &in, 1100u);
    EXPECT_EQ_U8(step.state, RTD_AWAIT_CONDITIONS);
    in.rtd_button = true;
    step = ecu_rtd_step(step.state, step.buzz_start_tick, &in, 1200u);
    EXPECT_EQ_U8(step.state, RTD_BUZZING);
    in.rtd_button = false;
    step = ecu_rtd_step(step.state, step.buzz_start_tick, &in, 4200u);
    EXPECT_EQ_U8(step.state, RTD_ENABLED);
    EXPECT_FALSE(step.buzzer_on);

    step = ecu_rtd_step(step.state, step.buzz_start_tick, &in, 4300u);
    EXPECT_EQ_U8(step.state, RTD_ENABLED);

    in.tsal = false;
    step = ecu_rtd_step(step.state, step.buzz_start_tick, &in, 4400u);
    EXPECT_EQ_U8(step.state, RTD_AWAIT_TSAL);
    EXPECT_TRUE(step.trip_pulse_requested);
}

static void test_rtd_wraparound_buzz_timer(void)
{
    ecu_rtd_inputs_t in = rtd_good_inputs();
    ecu_rtd_step_t step;
    uint32_t start = 0xFFFFFF00u;

    step = ecu_rtd_step(RTD_AWAIT_CONDITIONS, 0u, &in, start);
    EXPECT_EQ_U8(step.state, RTD_BUZZING);
    EXPECT_TRUE(step.buzzer_on);
    step = ecu_rtd_step(step.state, step.buzz_start_tick, &in, start + ECU_RTD_BUZZ_TIME_MS - 1u);
    EXPECT_EQ_U8(step.state, RTD_BUZZING);
    step = ecu_rtd_step(step.state, step.buzz_start_tick, &in, start + ECU_RTD_BUZZ_TIME_MS);
    EXPECT_EQ_U8(step.state, RTD_ENABLED);
}

static void test_rtd_early_or_stuck_press_cannot_auto_arm(void)
{
    ecu_rtd_inputs_t in = rtd_good_inputs();
    ecu_rtd_step_t step;

    in.brakelight = false;
    step = ecu_rtd_step(RTD_AWAIT_CONDITIONS, 0u, &in, 10u);
    EXPECT_EQ_U8(step.state, RTD_AWAIT_BUTTON_FALSE);
    in.brakelight = true;
    step = ecu_rtd_step(step.state, step.buzz_start_tick, &in, 20u);
    EXPECT_EQ_U8(step.state, RTD_AWAIT_BUTTON_FALSE);
    in.rtd_button = false;
    step = ecu_rtd_step(step.state, step.buzz_start_tick, &in, 30u);
    EXPECT_EQ_U8(step.state, RTD_AWAIT_CONDITIONS);
    in.rtd_button = true;
    step = ecu_rtd_step(step.state, step.buzz_start_tick, &in, 40u);
    EXPECT_EQ_U8(step.state, RTD_BUZZING);
}

static void test_cm200_disable_before_enable_and_reset_on_fault(void)
{
    uint8_t cycles = ECU_CM200_DISABLE_UNLOCK_CYCLES;
    uint8_t data[ECU_CM200_DATALEN];

    EXPECT_FALSE(ecu_cm200_update_unlock(false, &cycles));
    EXPECT_EQ_U8(cycles, ECU_CM200_DISABLE_UNLOCK_CYCLES);

    for(uint8_t i = 0u; i < ECU_CM200_DISABLE_UNLOCK_CYCLES; i++)
    {
        EXPECT_FALSE(ecu_cm200_update_unlock(true, &cycles));
    }
    EXPECT_EQ_U8(cycles, 0u);
    EXPECT_TRUE(ecu_cm200_update_unlock(true, &cycles));

    EXPECT_FALSE(ecu_cm200_update_unlock(false, &cycles));
    EXPECT_EQ_U8(cycles, ECU_CM200_DISABLE_UNLOCK_CYCLES);

    memset(data, 0xA5, sizeof(data));
    ecu_cm200_build_disable_packet(data);
    const uint8_t disabled[ECU_CM200_DATALEN] = {0u, 0u, 0u, 0u, 1u, 0u, 0u, 0u};
    EXPECT_MEM_EQ(data, disabled, ECU_CM200_DATALEN);

    ecu_cm200_build_torque_packet(data, 0x1234u);
    EXPECT_EQ_U8(data[0], 0x34u);
    EXPECT_EQ_U8(data[1], 0x12u);
    EXPECT_EQ_U8(data[4], 1u);
    EXPECT_EQ_U8(data[5], 1u);
    EXPECT_TRUE(ecu_cm200_packet_enabled(data));
    EXPECT_EQ_U32((uint16_t)ecu_cm200_packet_torque(data), 0x1234u);

    ecu_cm200_apply_rolling_counter(data, 0x0Au);
    EXPECT_EQ_U8(data[5], 0xA1u);
    EXPECT_EQ_U8(ecu_cm200_next_rolling_counter(0x0Eu), 0x0Fu);
    EXPECT_EQ_U8(ecu_cm200_next_rolling_counter(0x0Fu), 0u);

    ecu_cm200_build_disable_packet(data);
    ecu_cm200_apply_rolling_counter(data, 3u);
    EXPECT_EQ_U8(data[4], 1u);
    EXPECT_EQ_U8(data[5], 0x30u);

    ecu_cm200_build_torque_packet(data, -100);
    EXPECT_EQ_U8(data[0], 0x9Cu);
    EXPECT_EQ_U8(data[1], 0xFFu);
    EXPECT_EQ_U32((uint16_t)ecu_cm200_packet_torque(data), (uint16_t)-100);
}

static void test_cm200_supervisor_startup_runtime_and_wraparound(void)
{
    ecu_cm200_supervisor_t supervisor;

    ecu_cm200_supervisor_init(&supervisor);
    EXPECT_TRUE(ecu_cm200_supervisor_update(&supervisor, false, false, false, 0u));
    EXPECT_FALSE(supervisor.startup_timeout_latched);

    EXPECT_TRUE(ecu_cm200_supervisor_update(&supervisor, true, false, false, 100u));
    EXPECT_FALSE(supervisor.startup_timeout_latched);
    EXPECT_TRUE(ecu_cm200_supervisor_update(&supervisor,
                                            true,
                                            false,
                                            false,
                                            100u + ECU_CM200_STARTUP_GRACE_MS));
    EXPECT_FALSE(supervisor.startup_timeout_latched);
    EXPECT_TRUE(ecu_cm200_supervisor_update(&supervisor,
                                            true,
                                            false,
                                            false,
                                            101u + ECU_CM200_STARTUP_GRACE_MS));
    EXPECT_TRUE(supervisor.startup_timeout_latched);

    ecu_cm200_supervisor_init(&supervisor);
    EXPECT_TRUE(ecu_cm200_supervisor_update(&supervisor, true, false, false, 200u));
    EXPECT_FALSE(ecu_cm200_supervisor_update(&supervisor, true, true, false, 300u));
    EXPECT_TRUE(supervisor.ever_ready);
    EXPECT_TRUE(ecu_cm200_supervisor_update(&supervisor, true, false, false, 301u));
    EXPECT_TRUE(supervisor.runtime_fault_latched);
    EXPECT_TRUE(ecu_cm200_supervisor_update(&supervisor, false, false, false, 302u));
    EXPECT_TRUE(supervisor.runtime_fault_latched);

    ecu_cm200_supervisor_init(&supervisor);
    EXPECT_TRUE(ecu_cm200_supervisor_update(&supervisor,
                                            true,
                                            false,
                                            false,
                                            0xFFFFFF00u));
    EXPECT_TRUE(ecu_cm200_supervisor_update(&supervisor,
                                            true,
                                            false,
                                            false,
                                            0xFFFFFF00u + ECU_CM200_STARTUP_GRACE_MS + 1u));
    EXPECT_TRUE(supervisor.startup_timeout_latched);

    ecu_cm200_supervisor_init(&supervisor);
    EXPECT_TRUE(ecu_cm200_supervisor_update(&supervisor, true, false, true, 10u));
    EXPECT_TRUE(supervisor.runtime_fault_latched);
}

static void test_cm200_random_frame_fuzz_cannot_create_authority(void)
{
    static const uint16_t ids[] = {
        CM200_TEMPERATURES_1_CAN_ID,
        CM200_TEMPERATURES_2_CAN_ID,
        CM200_TEMPERATURES_3_CAN_ID,
        CM200_MOTOR_POSITION_CAN_ID,
        CM200_CURRENT_CAN_ID,
        CM200_VOLTAGE_CAN_ID,
        CM200_INTERNAL_STATES_CAN_ID,
        CM200_FAULTS_CAN_ID,
        CM200_TORQUE_TIMER_CAN_ID,
        CM200_FIRMWARE_CAN_ID,
        CM200_TORQUE_CAP_CAN_ID,
    };
    cm200_t cm;
    uint8_t frame[8];

    cm200_init(&cm);
    for(uint32_t cycle = 0u; cycle < ECU_HOST_LONG_FUZZ_CYCLES; cycle++)
    {
        for(uint8_t i = 0u; i < sizeof(frame); i++)
        {
            frame[i] = (uint8_t)(rng_next() >> 24u);
        }
        const uint32_t pick = rng_next();
        const uint16_t id = ids[pick % (sizeof(ids) / sizeof(ids[0]))];
        const bool standard = ((pick & 0x20u) == 0u);
        const uint8_t dlc = ((pick & 0x40u) == 0u) ? 8u : (uint8_t)(pick & 0x07u);
        (void)cm200_parse_can_frame(&cm, id, standard, dlc, frame, cycle);
        cm200_update_stale(&cm, cycle);

        /* Random broadcasts without a correlated transmitted command can
         * never manufacture torque authority. */
        EXPECT_FALSE(cm200_feedback_healthy(&cm));
        EXPECT_FALSE(cm200_allows_torque(&cm));
    }
    EXPECT_TRUE((cm.rx_count + cm.bad_rx_count) > 0u);
}

static void test_torque_slew_limiter_and_immediate_disable_path(void)
{
    EXPECT_EQ_U32((uint16_t)ecu_torque_slew_limit(0, 1000, 100u, 200u), 100u);
    EXPECT_EQ_U32((uint16_t)ecu_torque_slew_limit(950, 1000, 100u, 200u), 1000u);
    EXPECT_EQ_U32((uint16_t)ecu_torque_slew_limit(1000, 0, 100u, 200u), 800u);
    EXPECT_EQ_U32((uint16_t)ecu_torque_slew_limit(100, -100, 100u, 500u), (uint16_t)-100);
    EXPECT_EQ_U32((uint16_t)ecu_torque_slew_limit(123, 123, 100u, 200u), 123u);
}

static void test_bspd_semantics_and_discrete_recovery_filter(void)
{
    ecu_discrete_filter_t filter = {0};

    EXPECT_TRUE(ecu_bspd_raw_is_fault(false));
    EXPECT_FALSE(ecu_bspd_raw_is_fault(true));

    ecu_discrete_filter_init(&filter);
    EXPECT_TRUE(filter.faulted);
    for(uint8_t i = 0u; i < (ECU_DISCRETE_CLEAR_SAMPLES - 1u); i++)
    {
        EXPECT_TRUE(ecu_discrete_fault_update(&filter, false, ECU_DISCRETE_CLEAR_SAMPLES));
    }
    EXPECT_FALSE(ecu_discrete_fault_update(&filter, false, ECU_DISCRETE_CLEAR_SAMPLES));

    /* Assertion is immediate; clearing is delayed again. */
    EXPECT_TRUE(ecu_discrete_fault_update(&filter, true, ECU_DISCRETE_CLEAR_SAMPLES));
    EXPECT_TRUE(ecu_discrete_fault_update(&filter, false, ECU_DISCRETE_CLEAR_SAMPLES));
    EXPECT_TRUE(ecu_discrete_fault_update(NULL, false, ECU_DISCRETE_CLEAR_SAMPLES));
}

static void test_heartbeat_timeout_and_wraparound(void)
{
    EXPECT_FALSE(ecu_heartbeat_expired(100u, 350u, 250u));
    EXPECT_TRUE(ecu_heartbeat_expired(100u, 351u, 250u));
    EXPECT_FALSE(ecu_heartbeat_expired(0xFFFFFF00u, 0xFFFFFFFAu, 250u));
    EXPECT_TRUE(ecu_heartbeat_expired(0xFFFFFF00u, 0x00000010u, 250u));
}

static void test_torque_gate_fault_matrix(void)
{
    ecu_torque_inputs_t in = torque_good_inputs();
    EXPECT_TRUE(ecu_torque_allowed(&in));

    bool *faults[] = {
        &in.hard_fault,
        &in.apps_fault,
        &in.bppc_fault,
        &in.bse_fault,
        &in.ams_fault,
        &in.canbus_fault,
        &in.canbus_rx_fault,
        &in.canbus_tx_fault,
        &in.imd_fail,
        &in.bms_fail,
        &in.bspd_fail,
        &in.cm200_fault,
    };

    for(size_t i = 0u; i < (sizeof(faults) / sizeof(faults[0])); i++)
    {
        in = torque_good_inputs();
        *faults[i] = true;
        EXPECT_FALSE(ecu_torque_allowed(&in));
    }

    in = torque_good_inputs();
    in.cascadia_ok = false;
    EXPECT_FALSE(ecu_torque_allowed(&in));

    in = torque_good_inputs();
    in.rtd_mode = RTD_BUZZING;
    EXPECT_FALSE(ecu_torque_allowed(&in));
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
    run_test("AMS torque gate requires compact status", test_ams_torque_gate_requires_compact_status);
    run_test("AMS fault matrix blocks torque", test_ams_fault_matrix_blocks_torque);
    run_test("AMS protocol and sequence guards", test_ams_protocol_and_sequence_guards);
    run_test("AMS sequence fuzz stays fail-closed", test_ams_sequence_fuzz_stays_fail_closed);
    run_test("RTD state machine fault injection", test_rtd_state_machine_fault_injection);
    run_test("RTD wraparound buzz timer", test_rtd_wraparound_buzz_timer);
    run_test("RTD early or stuck press cannot auto arm", test_rtd_early_or_stuck_press_cannot_auto_arm);
    run_test("CM200 disable before enable and reset on fault", test_cm200_disable_before_enable_and_reset_on_fault);
    run_test("CM200 supervisor startup runtime and wraparound", test_cm200_supervisor_startup_runtime_and_wraparound);
    run_test("CM200 random frame fuzz cannot create authority", test_cm200_random_frame_fuzz_cannot_create_authority);
    run_test("torque slew limiter", test_torque_slew_limiter_and_immediate_disable_path);
    run_test("BSPD semantics and discrete recovery filter", test_bspd_semantics_and_discrete_recovery_filter);
    run_test("heartbeat timeout and wraparound", test_heartbeat_timeout_and_wraparound);
    run_test("torque gate fault matrix", test_torque_gate_fault_matrix);

    if(failures != 0)
    {
        printf("ECU SYSTEM SIL TESTS FAILED: %d failure(s)\n", failures);
        return 1;
    }

    printf("ALL ECU SYSTEM SIL TESTS PASSED\n");
    return 0;
}
