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
             faults->bspd_fail ||
             faults->cm200_fault);
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

static bool ecu_rtd_buzz_conditions_met(const ecu_rtd_inputs_t *inputs)
{
    if(inputs == NULL)
    {
        return false;
    }

    /* The driver's dedicated action is momentary.  Brake actuation and all
     * safety conditions must remain present through the ready-to-drive sound,
     * but holding the button is not required. */
    return (inputs->tsal &&
            inputs->cascadia_ok &&
            inputs->brakelight &&
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
            else if(inputs->rtd_button && ecu_rtd_conditions_met(inputs))
            {
                out.state = RTD_BUZZING;
                out.buzz_start_tick = now_ms;
                out.buzzer_on = true;
            }
            else if(inputs->rtd_button)
            {
                /* A press made before brake/controller/safety conditions are
                 * valid is consumed.  Require release and a new deliberate
                 * action; do not arm later from a held button. */
                out.state = RTD_AWAIT_BUTTON_FALSE;
            }
            break;

        case RTD_BUZZING:
            if(!inputs->tsal)
            {
                out.state = RTD_AWAIT_TSAL;
            }
            else if(!ecu_rtd_buzz_conditions_met(inputs))
            {
                out.state = RTD_AWAIT_BUTTON_FALSE;
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
            else if(!inputs->cascadia_ok || !ecu_faults_clear(&inputs->faults))
            {
                /* A new release/press cycle is required after every RTD loss;
                 * a held or stuck button cannot automatically re-enter RTD. */
                out.state = RTD_AWAIT_BUTTON_FALSE;
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
            !inputs->bspd_fail &&
            !inputs->cm200_fault);
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

bool ecu_cm200_packet_enabled(const uint8_t data[ECU_CM200_DATALEN])
{
    return ((data != NULL) && ((data[5] & 0x01u) != 0u));
}

int16_t ecu_cm200_packet_torque(const uint8_t data[ECU_CM200_DATALEN])
{
    uint16_t encoded;

    if(data == NULL)
    {
        return 0;
    }
    encoded = (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8u));
    return (int16_t)encoded;
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

int16_t ecu_torque_slew_limit(int16_t current_0p1nm,
                              int16_t target_0p1nm,
                              uint16_t rise_step_0p1nm,
                              uint16_t fall_step_0p1nm)
{
    int32_t current = current_0p1nm;
    int32_t target = target_0p1nm;

    if(target > current)
    {
        int32_t next = current + (int32_t)rise_step_0p1nm;
        return (int16_t)((next < target) ? next : target);
    }
    if(target < current)
    {
        int32_t next = current - (int32_t)fall_step_0p1nm;
        return (int16_t)((next > target) ? next : target);
    }
    return current_0p1nm;
}

void ecu_cm200_supervisor_init(ecu_cm200_supervisor_t *supervisor)
{
    if(supervisor == NULL)
    {
        return;
    }
    memset(supervisor, 0, sizeof(*supervisor));
}

bool ecu_cm200_supervisor_update(ecu_cm200_supervisor_t *supervisor,
                                 bool controller_on,
                                 bool feedback_ready,
                                 bool immediate_fault,
                                 uint32_t now_ms)
{
    if(supervisor == NULL)
    {
        return true;
    }

    if(!controller_on)
    {
        if(!supervisor->startup_timeout_latched && !supervisor->runtime_fault_latched)
        {
            supervisor->controller_power_seen = false;
            supervisor->ever_ready = false;
            supervisor->controller_power_tick = now_ms;
        }
        return true;
    }

    if(!supervisor->controller_power_seen)
    {
        supervisor->controller_power_seen = true;
        supervisor->controller_power_tick = now_ms;
    }

    if(immediate_fault)
    {
        supervisor->runtime_fault_latched = true;
    }
    else if(feedback_ready)
    {
        supervisor->ever_ready = true;
    }
    else if(supervisor->ever_ready)
    {
        supervisor->runtime_fault_latched = true;
    }
    else if((uint32_t)(now_ms - supervisor->controller_power_tick) >
            ECU_CM200_STARTUP_GRACE_MS)
    {
        supervisor->startup_timeout_latched = true;
    }

    return (!feedback_ready ||
            supervisor->startup_timeout_latched ||
            supervisor->runtime_fault_latched);
}

bool ecu_heartbeat_expired(uint32_t last_tick, uint32_t now_tick, uint32_t max_age_ms)
{
    return ((uint32_t)(now_tick - last_tick) > max_age_ms);
}
