/*
 * cm200.c
 *
 * Cascadia Motion CM200 CAN broadcast decoding and integrity supervision.
 */

#include <stddef.h>
#include <string.h>

#include "ext_drivers/cm200.h"

static uint16_t u16_le(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8u));
}

static int16_t s16_le(const uint8_t *data)
{
    return (int16_t)u16_le(data);
}

static uint32_t u32_le(const uint8_t *data)
{
    return ((uint32_t)data[0]) |
           ((uint32_t)data[1] << 8u) |
           ((uint32_t)data[2] << 16u) |
           ((uint32_t)data[3] << 24u);
}

static bool temp_sane(int16_t value)
{
    return ((value >= -500) && (value <= 2000));
}

static bool current_sane(int16_t value)
{
    return ((value >= -20000) && (value <= 20000));
}

static bool vsm_state_sane(uint8_t state)
{
    return ((state <= 7u) || (state == 14u) || (state == 15u));
}

static bool cm200_frame_from_id(uint32_t std_id, cm200_frame_index_t *frame)
{
    if(frame == NULL)
    {
        return false;
    }

    switch(std_id)
    {
        case CM200_TEMPERATURES_1_CAN_ID:
            *frame = CM200_FRAME_TEMPERATURES_1;
            return true;
        case CM200_TEMPERATURES_2_CAN_ID:
            *frame = CM200_FRAME_TEMPERATURES_2;
            return true;
        case CM200_TEMPERATURES_3_CAN_ID:
            *frame = CM200_FRAME_TEMPERATURES_3;
            return true;
        case CM200_MOTOR_POSITION_CAN_ID:
            *frame = CM200_FRAME_MOTOR_POSITION;
            return true;
        case CM200_CURRENT_CAN_ID:
            *frame = CM200_FRAME_CURRENT;
            return true;
        case CM200_VOLTAGE_CAN_ID:
            *frame = CM200_FRAME_VOLTAGE;
            return true;
        case CM200_INTERNAL_STATES_CAN_ID:
            *frame = CM200_FRAME_INTERNAL_STATES;
            return true;
        case CM200_FAULTS_CAN_ID:
            *frame = CM200_FRAME_FAULTS;
            return true;
        case CM200_TORQUE_TIMER_CAN_ID:
            *frame = CM200_FRAME_TORQUE_TIMER;
            return true;
        case CM200_FIRMWARE_CAN_ID:
            *frame = CM200_FRAME_FIRMWARE;
            return true;
        case CM200_TORQUE_CAP_CAN_ID:
            *frame = CM200_FRAME_TORQUE_CAPABILITY;
            return true;
        default:
            return false;
    }
}

static void mark_frame(cm200_t *dev,
                       cm200_frame_index_t frame,
                       bool sane,
                       uint32_t now_ms)
{
    cm200_frame_health_t *health = &dev->frame[frame];
    health->last_rx_tick = now_ms;
    health->rx_count++;
    health->valid = true;
    health->sane = sane;
    health->stale = false;
    dev->rx_count++;
}

static bool health_ok(const cm200_t *dev, cm200_frame_index_t frame)
{
    return (dev->frame[frame].valid &&
            dev->frame[frame].sane &&
            !dev->frame[frame].stale);
}

void cm200_init(cm200_t *dev)
{
    if(dev == NULL)
    {
        return;
    }

    memset(dev, 0, sizeof(*dev));
    for(uint8_t i = 0u; i < (uint8_t)CM200_FRAME_COUNT; i++)
    {
        dev->frame[i].stale = true;
    }
    dev->command_tx_stale = true;
}

bool cm200_is_known_can_id(uint32_t std_id)
{
    cm200_frame_index_t frame;
    return cm200_frame_from_id(std_id, &frame);
}

void cm200_invalidate_can_frame(cm200_t *dev, uint32_t std_id)
{
    cm200_frame_index_t frame;

    if((dev == NULL) || !cm200_frame_from_id(std_id, &frame))
    {
        return;
    }

    dev->frame[frame].valid = false;
    dev->frame[frame].sane = false;
    dev->frame[frame].stale = true;
}

static void update_counter_integrity(cm200_t *dev)
{
    bool exact;
    bool one_command_lag;
    bool correlated;

    if(!dev->have_command_tx)
    {
        return;
    }

    exact = (dev->inverter_expected_counter == dev->next_expected_counter);
    one_command_lag = (dev->inverter_expected_counter == dev->last_command_counter);
    correlated = (exact || one_command_lag);

    if(correlated)
    {
        if(!dev->have_counter_observation)
        {
            dev->have_counter_observation = true;
            dev->last_observed_expected_counter = dev->inverter_expected_counter;
        }
        else if(dev->inverter_expected_counter != dev->last_observed_expected_counter)
        {
            dev->last_observed_expected_counter = dev->inverter_expected_counter;
            if(dev->counter_progress_count < UINT8_MAX)
            {
                dev->counter_progress_count++;
            }
            dev->counter_synced = (dev->counter_progress_count != 0u);
        }
        dev->counter_mismatch_count = 0u;
        dev->counter_fault = false;
        return;
    }

    if(dev->counter_mismatch_count < UINT8_MAX)
    {
        dev->counter_mismatch_count++;
    }
    /* Before the first synchronization, the controller may legitimately be
     * expecting a counter left by an earlier sender.  The startup grace allows
     * one full 0..15 acquisition sweep; mismatch becomes a runtime fault only
     * after correlation has first been established. */
    dev->counter_fault = (dev->counter_synced &&
                          (dev->counter_mismatch_count >= CM200_INTEGRITY_MISMATCH_LIMIT));
}

static void update_torque_echo_integrity(cm200_t *dev)
{
    bool current_match;
    bool previous_match;

    if(!dev->have_command_tx)
    {
        return;
    }

    current_match = (dev->commanded_torque_0p1nm == dev->last_command_torque_0p1nm);
    previous_match = (dev->have_previous_command_tx &&
                      (dev->commanded_torque_0p1nm == dev->previous_command_torque_0p1nm));

    if(current_match || (dev->torque_echo_synced && previous_match))
    {
        dev->torque_echo_synced = true;
        dev->torque_echo_mismatch_count = 0u;
        dev->torque_echo_fault = false;
        return;
    }

    if(dev->torque_echo_mismatch_count < UINT8_MAX)
    {
        dev->torque_echo_mismatch_count++;
    }
    dev->torque_echo_fault =
        (dev->torque_echo_synced &&
         (dev->torque_echo_mismatch_count >= CM200_INTEGRITY_MISMATCH_LIMIT));
}

static void update_power_on_timer(cm200_t *dev, uint32_t timer)
{
    if(dev->frame[CM200_FRAME_TORQUE_TIMER].valid)
    {
        uint32_t delta = timer - dev->power_on_timer;
        dev->previous_power_on_timer = dev->power_on_timer;

        if(delta == 0u)
        {
            if(dev->timer_repeat_count < UINT8_MAX)
            {
                dev->timer_repeat_count++;
            }
            if(dev->timer_repeat_count >= CM200_TIMER_REPEAT_LIMIT)
            {
                dev->timer_reset_fault = true;
            }
        }
        else if(delta > 0x80000000u)
        {
            /* A small backwards jump is a controller reset/replay.  A natural
             * uint32_t wrap produces a small positive modular delta. */
            dev->timer_reset_fault = true;
            dev->timer_repeat_count = 0u;
        }
        else
        {
            dev->timer_repeat_count = 0u;
            dev->timer_observed_progress = true;
        }
    }

    dev->power_on_timer = timer;
}

bool cm200_parse_can_frame(cm200_t *dev,
                           uint32_t std_id,
                           bool is_standard,
                           uint8_t dlc,
                           const uint8_t *data,
                           uint32_t now_ms)
{
    cm200_frame_index_t frame;
    bool sane = true;

    if(dev == NULL)
    {
        return false;
    }

    if(!cm200_frame_from_id(std_id, &frame))
    {
        return false;
    }

    if((data == NULL) || !is_standard || (dlc != CM200_FRAME_DLC))
    {
        dev->bad_rx_count++;
        cm200_invalidate_can_frame(dev, std_id);
        return false;
    }

    switch(std_id)
    {
        case CM200_TEMPERATURES_1_CAN_ID:
            dev->module_a_temp_0p1c = s16_le(&data[0]);
            dev->module_b_temp_0p1c = s16_le(&data[2]);
            dev->module_c_temp_0p1c = s16_le(&data[4]);
            dev->gate_driver_temp_0p1c = s16_le(&data[6]);
            sane = (temp_sane(dev->module_a_temp_0p1c) &&
                    temp_sane(dev->module_b_temp_0p1c) &&
                    temp_sane(dev->module_c_temp_0p1c) &&
                    temp_sane(dev->gate_driver_temp_0p1c));
            break;

        case CM200_TEMPERATURES_2_CAN_ID:
            dev->control_board_temp_0p1c = s16_le(&data[0]);
            dev->rtd1_temp_0p1c = s16_le(&data[2]);
            dev->rtd2_temp_0p1c = s16_le(&data[4]);
            dev->motor_hotspot_temp_0p1c = s16_le(&data[6]);
            sane = (temp_sane(dev->control_board_temp_0p1c) &&
                    temp_sane(dev->rtd1_temp_0p1c) &&
                    temp_sane(dev->rtd2_temp_0p1c) &&
                    temp_sane(dev->motor_hotspot_temp_0p1c));
            break;

        case CM200_TEMPERATURES_3_CAN_ID:
            dev->coolant_temp_0p1c = s16_le(&data[0]);
            dev->inverter_hotspot_temp_0p1c = s16_le(&data[2]);
            dev->motor_temp_0p1c = s16_le(&data[4]);
            dev->torque_shudder_0p1nm = s16_le(&data[6]);
            sane = (temp_sane(dev->coolant_temp_0p1c) &&
                    temp_sane(dev->inverter_hotspot_temp_0p1c) &&
                    temp_sane(dev->motor_temp_0p1c));
            break;

        case CM200_MOTOR_POSITION_CAN_ID:
            dev->motor_angle = s16_le(&data[0]);
            dev->motor_speed_rpm = s16_le(&data[2]);
            dev->electrical_frequency_0p1hz = s16_le(&data[4]);
            dev->resolver_delta = s16_le(&data[6]);
            break;

        case CM200_CURRENT_CAN_ID:
            dev->phase_a_current_0p1a = s16_le(&data[0]);
            dev->phase_b_current_0p1a = s16_le(&data[2]);
            dev->phase_c_current_0p1a = s16_le(&data[4]);
            dev->dc_bus_current_0p1a = s16_le(&data[6]);
            sane = (current_sane(dev->phase_a_current_0p1a) &&
                    current_sane(dev->phase_b_current_0p1a) &&
                    current_sane(dev->phase_c_current_0p1a) &&
                    current_sane(dev->dc_bus_current_0p1a));
            break;

        case CM200_VOLTAGE_CAN_ID:
            dev->dc_bus_voltage_0p1v = s16_le(&data[0]);
            dev->output_voltage_0p1v = s16_le(&data[2]);
            dev->vd_voltage_0p1v = s16_le(&data[4]);
            dev->vq_voltage_0p1v = s16_le(&data[6]);
            sane = ((dev->dc_bus_voltage_0p1v >= 0) &&
                    (dev->dc_bus_voltage_0p1v <= 10000));
            break;

        case CM200_INTERNAL_STATES_CAN_ID:
            dev->vsm_state = data[0];
            dev->pwm_frequency_khz = data[1];
            dev->inverter_state = data[2];
            dev->relay_states = data[3];
            dev->mode_states = data[4];
            dev->command_states = data[5];
            dev->enable_states = data[6];
            dev->limit_states = data[7];
            dev->torque_mode = ((data[4] & 0x01u) == 0u);
            dev->command_mode_can = ((data[5] & 0x01u) == 0u);
            dev->inverter_expected_counter = (uint8_t)((data[5] >> 4u) & 0x0Fu);
            dev->inverter_enabled = ((data[6] & 0x01u) != 0u);
            dev->inverter_enable_lockout = ((data[6] & 0x80u) != 0u);
            dev->forward_direction = ((data[7] & 0x01u) != 0u);
            dev->bms_active = ((data[7] & 0x02u) != 0u);
            sane = (vsm_state_sane(dev->vsm_state) && (dev->inverter_state <= 12u));
            update_counter_integrity(dev);
            break;

        case CM200_FAULTS_CAN_ID:
            dev->post_faults = u32_le(&data[0]);
            dev->run_faults = u32_le(&data[4]);
            break;

        case CM200_TORQUE_TIMER_CAN_ID:
            dev->commanded_torque_0p1nm = s16_le(&data[0]);
            dev->torque_feedback_0p1nm = s16_le(&data[2]);
            update_power_on_timer(dev, u32_le(&data[4]));
            update_torque_echo_integrity(dev);
            break;

        case CM200_FIRMWARE_CAN_ID:
            dev->eeprom_project_code = u16_le(&data[0]);
            dev->software_version = u16_le(&data[2]);
            dev->date_mmdd = u16_le(&data[4]);
            dev->date_year = u16_le(&data[6]);
            break;

        case CM200_TORQUE_CAP_CAN_ID:
            dev->motor_torque_available_0p1nm = s16_le(&data[0]);
            dev->regen_torque_available_0p1nm = s16_le(&data[2]);
            sane = (dev->motor_torque_available_0p1nm >= 0);
            break;

        default:
            return false;
    }

    mark_frame(dev, frame, sane, now_ms);
    return true;
}

void cm200_update_stale(cm200_t *dev, uint32_t now_ms)
{
    if(dev == NULL)
    {
        return;
    }

    for(uint8_t i = 0u; i < (uint8_t)CM200_FRAME_COUNT; i++)
    {
        uint32_t timeout = CM200_FAST_STALE_TIMEOUT_MS;
        if((i == (uint8_t)CM200_FRAME_TEMPERATURES_1) ||
           (i == (uint8_t)CM200_FRAME_TEMPERATURES_2) ||
           (i == (uint8_t)CM200_FRAME_TEMPERATURES_3))
        {
            timeout = CM200_SLOW_STALE_TIMEOUT_MS;
        }
        else if(i == (uint8_t)CM200_FRAME_FIRMWARE)
        {
            timeout = CM200_FIRMWARE_STALE_TIMEOUT_MS;
        }

        dev->frame[i].stale = (!dev->frame[i].valid ||
            ((uint32_t)(now_ms - dev->frame[i].last_rx_tick) > timeout));
    }

    dev->command_tx_stale = (!dev->have_command_tx ||
        ((uint32_t)(now_ms - dev->last_command_tx_tick) > CM200_COMMAND_STALE_TIMEOUT_MS));
}

void cm200_note_command_tx(cm200_t *dev,
                           uint8_t rolling_counter,
                           bool enabled,
                           int16_t torque_0p1nm,
                           uint32_t now_ms)
{
    if(dev == NULL)
    {
        return;
    }

    if(dev->have_command_tx)
    {
        dev->previous_command_torque_0p1nm = dev->last_command_torque_0p1nm;
        dev->have_previous_command_tx = true;
    }

    dev->last_command_counter = (uint8_t)(rolling_counter & 0x0Fu);
    dev->next_expected_counter = (uint8_t)((dev->last_command_counter + 1u) & 0x0Fu);
    dev->last_command_enabled = enabled;
    dev->last_command_torque_0p1nm = torque_0p1nm;
    dev->last_command_tx_tick = now_ms;
    dev->command_tx_count++;
    dev->command_tx_stale = false;
    dev->have_command_tx = true;
}

bool cm200_has_immediate_fault(const cm200_t *dev)
{
    if(dev == NULL)
    {
        return true;
    }

    return ((dev->frame[CM200_FRAME_FAULTS].valid &&
             ((dev->post_faults != 0u) || (dev->run_faults != 0u))) ||
            (dev->frame[CM200_FRAME_INTERNAL_STATES].valid &&
             ((dev->vsm_state == 7u) || (dev->vsm_state == 15u))) ||
            dev->counter_fault ||
            dev->torque_echo_fault ||
            dev->timer_reset_fault);
}

bool cm200_feedback_healthy(const cm200_t *dev)
{
    if(dev == NULL)
    {
        return false;
    }

    return (health_ok(dev, CM200_FRAME_MOTOR_POSITION) &&
            health_ok(dev, CM200_FRAME_VOLTAGE) &&
            health_ok(dev, CM200_FRAME_INTERNAL_STATES) &&
            health_ok(dev, CM200_FRAME_FAULTS) &&
            health_ok(dev, CM200_FRAME_TORQUE_TIMER) &&
            health_ok(dev, CM200_FRAME_TORQUE_CAPABILITY) &&
            !dev->command_tx_stale &&
            dev->counter_synced &&
            !dev->counter_fault &&
            dev->torque_echo_synced &&
            !dev->torque_echo_fault &&
            dev->timer_observed_progress &&
            !dev->timer_reset_fault &&
            dev->torque_mode &&
            dev->command_mode_can &&
            !dev->inverter_enable_lockout &&
            (dev->post_faults == 0u) &&
            (dev->run_faults == 0u));
}

bool cm200_allows_torque(const cm200_t *dev)
{
    if(dev == NULL)
    {
        return false;
    }

    return (cm200_feedback_healthy(dev) &&
            ((dev->vsm_state == 5u) || (dev->vsm_state == 6u)) &&
            dev->forward_direction);
}

int16_t cm200_clamp_motoring_torque(const cm200_t *dev, int16_t requested_0p1nm)
{
    if((dev == NULL) || (requested_0p1nm <= 0) ||
       !health_ok(dev, CM200_FRAME_TORQUE_CAPABILITY) ||
       (dev->motor_torque_available_0p1nm <= 0))
    {
        return 0;
    }

    if(requested_0p1nm > dev->motor_torque_available_0p1nm)
    {
        return dev->motor_torque_available_0p1nm;
    }
    return requested_0p1nm;
}

uint32_t cm200_frame_age_ms(const cm200_t *dev, cm200_frame_index_t frame, uint32_t now_ms)
{
    if((dev == NULL) || (frame >= CM200_FRAME_COUNT) || !dev->frame[frame].valid)
    {
        return UINT32_MAX;
    }
    return (uint32_t)(now_ms - dev->frame[frame].last_rx_tick);
}
