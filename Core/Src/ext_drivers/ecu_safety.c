/*
 * ecu_safety.c
 *
 * Pure ECU safety helpers used by production tasks and host SIL tests.
 */

#include <stddef.h>
#include <string.h>

#include "ext_drivers/ecu_safety.h"
#include "ecu_config.h"

#define TO_LSB_U16(x) ((uint8_t)((x) & 0xFFu))
#define TO_MSB_U16(x) ((uint8_t)(((x) >> 8u) & 0xFFu))

bool ecu_faults_clear(const ecu_fault_inputs_t *faults)
{
    if(faults == NULL)
    {
        return false;
    }

    return !(faults->hard_fault ||
             faults->apps_fault ||
             faults->bse_fault ||
             faults->bppc_fault ||
             faults->ams_fault ||
             faults->canbus_fault ||
             faults->canbus_rx_fault ||
             faults->canbus_tx_fault ||
             faults->imd_fail ||
             faults->bms_fail ||
             faults->bspd_fail);
}

bool ecu_rtd_conditions_met(const ecu_rtd_inputs_t *inputs)
{
    if(inputs == NULL)
    {
        return false;
    }

    return (inputs->tsal &&
            inputs->cascadia_ok &&
            inputs->brakelight &&
            inputs->rtd_button &&
            ecu_faults_clear(&inputs->faults));
}

ecu_rtd_step_t ecu_rtd_step(rtd_state_t state,
                            uint32_t buzz_start_tick,
                            const ecu_rtd_inputs_t *inputs,
                            uint32_t now_ms)
{
    ecu_rtd_step_t out = {
        .state = state,
        .buzz_start_tick = buzz_start_tick,
        .buzzer_on = false,
        .trip_pulse_requested = false,
    };

    if(inputs == NULL)
    {
        out.state = RTD_AWAIT_TSAL;
        return out;
    }

    switch(state)
    {
        case RTD_AWAIT_TSAL:
            if(inputs->tsal)
            {
                out.state = RTD_AWAIT_BUTTON_FALSE;
            }
            break;

        case RTD_AWAIT_BUTTON_FALSE:
            if(!inputs->tsal)
            {
                out.state = RTD_AWAIT_TSAL;
            }
            else if(!inputs->rtd_button)
            {
                out.state = RTD_AWAIT_CONDITIONS;
            }
            break;

        case RTD_AWAIT_CONDITIONS:
            if(!inputs->tsal)
            {
                out.state = RTD_AWAIT_TSAL;
            }
            else if(ecu_rtd_conditions_met(inputs))
            {
                out.state = RTD_BUZZING;
                out.buzz_start_tick = now_ms;
                out.buzzer_on = true;
            }
            break;

        case RTD_BUZZING:
            if(!inputs->tsal)
            {
                out.state = RTD_AWAIT_TSAL;
            }
            else if(!ecu_rtd_conditions_met(inputs))
            {
                out.state = RTD_AWAIT_CONDITIONS;
            }
            else if((uint32_t)(now_ms - buzz_start_tick) >= ECU_RTD_BUZZ_TIME_MS)
            {
                out.state = RTD_ENABLED;
            }
            else
            {
                out.state = RTD_BUZZING;
                out.buzzer_on = true;
            }
            break;

        case RTD_ENABLED:
            if(!inputs->tsal)
            {
                out.state = RTD_AWAIT_TSAL;
            }
            else if(!inputs->cascadia_ok || !inputs->rtd_button || !ecu_faults_clear(&inputs->faults))
            {
                out.state = RTD_AWAIT_CONDITIONS;
            }

            if(out.state != RTD_ENABLED)
            {
                out.trip_pulse_requested = true;
            }
            break;

        default:
            out.state = RTD_AWAIT_TSAL;
            break;
    }

    if(out.state != RTD_BUZZING)
    {
        out.buzzer_on = false;
    }

    return out;
}

bool ecu_torque_allowed(const ecu_torque_inputs_t *inputs)
{
    if(inputs == NULL)
    {
        return false;
    }

    return (inputs->cascadia_ok &&
            !inputs->hard_fault &&
            !inputs->apps_fault &&
            (inputs->rtd_mode == RTD_ENABLED) &&
            !inputs->bppc_fault &&
            !inputs->bse_fault &&
            !inputs->ams_fault &&
            !inputs->canbus_fault &&
            !inputs->canbus_rx_fault &&
            !inputs->canbus_tx_fault &&
            !inputs->imd_fail &&
            !inputs->bms_fail &&
            !inputs->bspd_fail);
}

void ecu_discrete_filter_init(ecu_discrete_filter_t *filter)
{
    if(filter == NULL)
    {
        return;
    }

    filter->initialized = true;
    filter->faulted = true;
    filter->healthy_samples = 0u;
}

bool ecu_discrete_fault_update(ecu_discrete_filter_t *filter,
                               bool raw_fault,
                               uint8_t clear_samples)
{
    if(filter == NULL)
    {
        return true;
    }

    if(!filter->initialized)
    {
        ecu_discrete_filter_init(filter);
    }

    if(raw_fault || (clear_samples == 0u))
    {
        filter->faulted = true;
        filter->healthy_samples = 0u;
        return true;
    }

    if(filter->healthy_samples < clear_samples)
    {
        filter->healthy_samples++;
    }

    if(filter->healthy_samples >= clear_samples)
    {
        filter->faulted = false;
    }

    return filter->faulted;
}

bool ecu_bspd_raw_is_fault(bool raw_pin_high)
{
#if ECU_BSPD_OK_ACTIVE_HIGH
    return !raw_pin_high;
#else
    return raw_pin_high;
#endif
}

void ecu_cm200_build_disable_packet(uint8_t data[ECU_CM200_DATALEN])
{
    if(data == NULL)
    {
        return;
    }

    memset(data, 0, ECU_CM200_DATALEN);
    /* Do not change direction during a disable transition; CM200 treats a
     * direction change while enabled as a lockout condition. */
    data[4] = ECU_CM200_FORWARD_DIRECTION;
    data[5] = 0u; /* Inverter enable bit = disabled. */
}

void ecu_cm200_build_torque_packet(uint8_t data[ECU_CM200_DATALEN], int16_t torque_cmd)
{
    uint16_t encoded;

    if(data == NULL)
    {
        return;
    }

    memset(data, 0, ECU_CM200_DATALEN);
    encoded = (uint16_t)torque_cmd;
    data[0] = TO_LSB_U16(encoded);
    data[1] = TO_MSB_U16(encoded);
    data[2] = 0u;
    data[3] = 0u;
    data[4] = ECU_CM200_FORWARD_DIRECTION;
    data[5] = 1u; /* Inverter Enable: 0-disable, 1-enable. */
    data[6] = 0u;
    data[7] = 0u;
}

void ecu_cm200_apply_rolling_counter(uint8_t data[ECU_CM200_DATALEN], uint8_t rolling_counter)
{
    if(data == NULL)
    {
        return;
    }

    data[5] = (uint8_t)((data[5] & ECU_CM200_CONTROL_MASK) |
                       (uint8_t)((rolling_counter & 0x0Fu) << 4u));
}

uint8_t ecu_cm200_next_rolling_counter(uint8_t rolling_counter)
{
    return (uint8_t)((rolling_counter + 1u) & 0x0Fu);
}

bool ecu_cm200_update_unlock(bool torque_allowed, uint8_t *disable_unlock_cycles)
{
    if(disable_unlock_cycles == NULL)
    {
        return false;
    }

    if(!torque_allowed)
    {
        *disable_unlock_cycles = ECU_CM200_DISABLE_UNLOCK_CYCLES;
        return false;
    }

    if(*disable_unlock_cycles > 0u)
    {
        (*disable_unlock_cycles)--;
        return false;
    }

    return true;
}

bool ecu_heartbeat_expired(uint32_t last_tick, uint32_t now_tick, uint32_t max_age_ms)
{
    return ((uint32_t)(now_tick - last_tick) > max_age_ms);
}
