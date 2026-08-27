#include "support/fake_hal.h"

#include "ext_drivers/ams.h"
#include "ext_drivers/canbus.h"
#include "ext_drivers/cli.h"
#include "ext_drivers/cm200.h"
#include "ext_drivers/ecu_safety.h"
#include "ext_drivers/flow_sensor.h"
#include "ext_drivers/poten.h"
#include "ext_drivers/pressure_sensor.h"
#include "ext_drivers/pwm.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef ECU_CROSS_STRESS_CYCLES
#define ECU_CROSS_STRESS_CYCLES 200000u
#endif

static uint32_t rng_state = 0xC0FFEE01u;
static uint32_t rng32(void)
{
    uint32_t x = rng_state;
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    rng_state = x;
    return x;
}

static bool manual_torque_allowed(const ecu_torque_inputs_t *i)
{
    return i->cascadia_ok && !i->hard_fault && !i->apps_fault &&
           !i->bppc_fault && !i->bse_fault && !i->ams_fault &&
           !i->canbus_fault && !i->canbus_rx_fault && !i->canbus_tx_fault &&
           !i->imd_fail && !i->bms_fail && !i->bspd_fail && !i->cm200_fault &&
           i->rtd_mode == RTD_ENABLED;
}

static bool ams_authority_invariant(const ams_t *a)
{
    if(!ams_allows_torque(a)) return true;
    return !a->stale && !a->compact_status_stale &&
           a->compact_status_valid && a->compact_protocol_valid &&
           a->compact_electrical_valid && a->compact_electrical_sane &&
           !a->compact_electrical_stale && a->compact_thermal_valid &&
           a->compact_thermal_sane && !a->compact_thermal_stale &&
           ((a->thermal_flags & AMS_THERMAL_TORQUE_BLOCK_MASK) == 0u) &&
           a->bms_ok && !a->bms_inhibited && !a->ams_hard_fault &&
           !a->ams_soft_fault && a->voltage_valid && a->current_valid &&
           a->temp_valid && !a->ams_can_fault && !a->voltage_fault &&
           !a->temp_fault && !a->current_fault && !a->charger_fault &&
           !a->adbms_diag_fault && !a->task_heartbeat_fault &&
           !a->logger_heartbeat_fault && !a->compact_sequence_fault;
}

int main(void)
{
    host_hal_reset();
    ADC_HandleTypeDef adc = {0};
    TIM_HandleTypeDef htim = {.Instance=&host_tim5};
    CAN_HandleTypeDef hcan = {0};
    CAN_TxHeaderTypeDef txh = {0};
    poten_t pot;
    pressure_sensor_t pressure;
    pwm_t pwm;
    flow_sensor_t flow;
    canbus_t bus;
    ams_t ams;
    cm200_t cm;
    volatile uint32_t ccr = 0u;

    poten_init(&pot, 200u, 3800u, &adc);
    pressure_sensor_init(&pressure, 300u, 3700u, &adc, 1u);
    if(pwm_device_init(&pwm, &host_tim3, &htim, 65535u, &ccr, 1) != 0)
    {
        puts("FAIL stress setup: PWM");
        return 1;
    }
    flow_sensor_init(&flow, 108000000u, &htim, &host_tim5,
                     TIM_CHANNEL_2, TIM_CHANNEL_1);
    canbus_device_init(&bus, &hcan, &txh);
    ams_init(&ams);
    cm200_init(&cm);

    uint32_t failures = 0u;
    uint32_t last_seed = rng_state;

    for(uint32_t cycle=0u; cycle<ECU_CROSS_STRESS_CYCLES; cycle++)
    {
        last_seed = rng_state;
        uint32_t r = rng32();

        pot.count = (uint16_t)r;
        float pp = poten_get_raw_percent(&pot);
        if(!isfinite(pp) || pp < 0.0f || pp > 100.0f) failures++;

        pressure.count = (uint16_t)(r >> 8u);
        float bp = pressure_sensor_get_percent(&pressure);
        if(!isfinite(bp) || bp < 0.0f || bp > 100.0f) failures++;

        float requested;
        if((cycle % 997u) == 0u) requested = NAN;
        else if((cycle % 991u) == 0u) requested = INFINITY;
        else requested = ((float)(int32_t)r / 10000000.0f);
        uint32_t old_ccr = ccr;
        int pwm_ret = pwm_set_percent(&pwm, requested);
        if(!isfinite(requested))
        {
            if(pwm_ret == 0 || ccr != old_ccr) failures++;
        }
        else
        {
            if(pwm_ret != 0 || ccr > 65535u || pwm.duty_cycle < 0.0f ||
               pwm.duty_cycle > 100.0f) failures++;
        }

        uint32_t total = rng32() & 0xFFFFu;
        uint32_t high = rng32() & 0xFFFFu;
        host_hal.capture[0] = total;
        host_hal.capture[1] = high;
        int flow_ret = flow_sensor_read(&flow);
        if(total == 0u)
        {
            if(flow_ret != 0 || flow.valid || !flow.stale) failures++;
        }
        else if(high > total)
        {
            if(flow_ret == 0 || flow.valid || !flow.stale) failures++;
        }
        else
        {
            if(flow_ret != 0 || !flow.valid || flow.stale ||
               !isfinite(flow.duty) || !isfinite(flow.freq) ||
               flow.duty < 0.0f || flow.duty > 100.0f) failures++;
        }

        ecu_torque_inputs_t ti = {
            .cascadia_ok = (r & (1u<<0)) != 0u,
            .hard_fault = (r & (1u<<1)) != 0u,
            .apps_fault = (r & (1u<<2)) != 0u,
            .bppc_fault = (r & (1u<<3)) != 0u,
            .bse_fault = (r & (1u<<4)) != 0u,
            .ams_fault = (r & (1u<<5)) != 0u,
            .canbus_fault = (r & (1u<<6)) != 0u,
            .canbus_rx_fault = (r & (1u<<7)) != 0u,
            .canbus_tx_fault = (r & (1u<<8)) != 0u,
            .imd_fail = (r & (1u<<9)) != 0u,
            .bms_fail = (r & (1u<<10)) != 0u,
            .bspd_fail = (r & (1u<<11)) != 0u,
            .cm200_fault = (r & (1u<<12)) != 0u,
            .rtd_mode = (rtd_state_t)(r % 5u),
        };
        if(ecu_torque_allowed(&ti) != manual_torque_allowed(&ti)) failures++;

        int16_t torque = (int16_t)(r >> 16u);
        uint8_t packet[ECU_CM200_DATALEN];
        ecu_cm200_build_torque_packet(packet, torque);
        uint8_t counter = (uint8_t)(r & 0x0Fu);
        ecu_cm200_apply_rolling_counter(packet, counter);
        if(!ecu_cm200_packet_enabled(packet) ||
           ecu_cm200_packet_torque(packet) != torque ||
           ecu_cm200_next_rolling_counter(counter) != (uint8_t)((counter + 1u) & 0x0Fu))
            failures++;

        canbus_tx_request_t req;
        memset(&req, 0, sizeof(req));
        req.packet.id = r & 0x7FFu;
        memcpy(req.packet.data, &r, sizeof(r));
        if(canbus_queue_tx(&bus, &req) != HAL_OK) failures++;
        canbus_tx_request_t saved;
        memcpy(&saved, bus.tx_queue_storage, sizeof(saved));
        if(saved.packet.id != req.packet.id ||
           memcmp(saved.packet.data, req.packet.data, sizeof(req.packet.data)) != 0)
            failures++;

        uint8_t frame[8];
        for(size_t i=0; i<sizeof(frame); i++) frame[i] = (uint8_t)rng32();
        const uint32_t ids[] = {
            AMS_ECU_STATUS_CANBUS_ID, AMS_ECU_ELECTRICAL_CANBUS_ID,
            AMS_ECU_THERMAL_CANBUS_ID, AMS_ECU_HEALTH_CANBUS_ID,
            AMS_TELEM_CANBUS_ID, AMS_ESTIMATOR_CANBUS_ID, 0x123u
        };
        uint32_t id = ids[rng32() % (sizeof(ids)/sizeof(ids[0]))];
        (void)ams_parse_can_frame(&ams, id, (rng32() & 1u) != 0u,
                                  (uint8_t)(rng32() % 10u), frame, cycle);
        ams_update_stale(&ams, cycle);
        if(!ams_authority_invariant(&ams)) failures++;

        if(failures != 0u)
        {
            printf("FAIL cross-module stress cycle=%lu seed=0x%08lX failures=%lu\n",
                   (unsigned long)cycle, (unsigned long)last_seed,
                   (unsigned long)failures);
            return 1;
        }
    }

    printf("PASS ECU cross-module deterministic stress: %lu cycles final_seed=0x%08lX\n",
           (unsigned long)ECU_CROSS_STRESS_CYCLES,
           (unsigned long)rng_state);
    return 0;
}
