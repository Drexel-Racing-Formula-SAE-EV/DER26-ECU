/*
 * cooling_control.c
 *
 * SEN-04-5 conversion uses the two 1 kohm sense resistors populated on the
 * ECU miscellaneous board and the manufacturer's published typical points:
 * 100 F = 500 ohm, 200 F = 75 ohm, 300 F = 19 ohm.  The resulting
 * Steinhart-Hart fit is an implementation starting point; target release still
 * requires a measured bath calibration and remains separately gated.
 */
#include "ext_drivers/cooling_control.h"

#include <math.h>
#include <stddef.h>

#define ECU_ADC_FULL_SCALE_COUNTS       4095.0f
#define ECU_ADC_REFERENCE_V             3.3f
#define ECU_SEN04_5_PULLUP_OHM          1000.0f
#define ECU_SEN04_5_SHORT_COUNT_MAX     8u
#define ECU_SEN04_5_OPEN_COUNT_MIN      4087u
#define ECU_SEN04_5_SH_A                1.58954030e-3f
#define ECU_SEN04_5_SH_B                2.65781670e-4f
#define ECU_SEN04_5_SH_C               -1.04527165e-7f
#define ECU_SEN04_5_MIN_C              -25.0f
#define ECU_SEN04_5_MAX_C              155.0f

#define ECU_PRESS_SENSOR_DIVIDER_GAIN   1.5f
#define ECU_PRESS_SENSOR_MIN_V          0.35f
#define ECU_PRESS_SENSOR_MAX_V          4.65f

#define ECU_BV2000_PULSES_PER_LITER     750.0f
#define ECU_BV2000_MIN_LPM              2.0f
#define ECU_BV2000_MAX_LPM              35.0f

#define ECU_COOLING_TRIP_SAMPLES        3u
#define ECU_COOLING_CLEAR_SAMPLES       10u
#define ECU_COOLING_STARTUP_SAMPLES     25u /* 5 s at COOL_FREQ=5 Hz. */
#define ECU_COOLING_MAX_FLUID_C         85.0f
#define ECU_COOLING_CLEAR_FLUID_C       80.0f
#define ECU_COOLING_MIN_FLOW_LPM        5.0f
#define ECU_COOLING_MAX_PRESSURE_PSI    40.0f
#define ECU_COOLING_MAX_TEMP_DELTA_C    25.0f

static float clampf_local(float value, float low, float high)
{
    if(value < low) return low;
    if(value > high) return high;
    return value;
}

float ecu_coolant_temp_sen04_5_from_adc(uint16_t count, bool *valid)
{
    if(valid != NULL) *valid = false;
    if((count <= ECU_SEN04_5_SHORT_COUNT_MAX) ||
       (count >= ECU_SEN04_5_OPEN_COUNT_MIN))
    {
        return NAN;
    }

    const float denominator = ECU_ADC_FULL_SCALE_COUNTS - (float)count;
    if(denominator <= 0.0f)
    {
        return NAN;
    }
    const float resistance_ohm =
        ECU_SEN04_5_PULLUP_OHM * (float)count / denominator;
    if(!isfinite(resistance_ohm) || (resistance_ohm <= 0.0f))
    {
        return NAN;
    }

    const float log_r = logf(resistance_ohm);
    const float inv_kelvin = ECU_SEN04_5_SH_A +
        (ECU_SEN04_5_SH_B * log_r) +
        (ECU_SEN04_5_SH_C * log_r * log_r * log_r);
    if(!isfinite(inv_kelvin) || (inv_kelvin <= 0.0f))
    {
        return NAN;
    }

    const float temperature_c = (1.0f / inv_kelvin) - 273.15f;
    if(!isfinite(temperature_c) ||
       (temperature_c < ECU_SEN04_5_MIN_C) ||
       (temperature_c > ECU_SEN04_5_MAX_C))
    {
        return NAN;
    }
    if(valid != NULL) *valid = true;
    return temperature_c;
}

float ecu_coolant_pressure_100psi_from_adc(uint16_t count, bool *valid)
{
    if(valid != NULL) *valid = false;
    if(count > (uint16_t)ECU_ADC_FULL_SCALE_COUNTS)
    {
        return NAN;
    }

    const float adc_v = ((float)count * ECU_ADC_REFERENCE_V) /
                        ECU_ADC_FULL_SCALE_COUNTS;
    const float sensor_v = adc_v * ECU_PRESS_SENSOR_DIVIDER_GAIN;
    if(!isfinite(sensor_v) || (sensor_v < ECU_PRESS_SENSOR_MIN_V) ||
       (sensor_v > ECU_PRESS_SENSOR_MAX_V))
    {
        return NAN;
    }

    const float pressure_psi = 25.0f * (sensor_v - 0.5f);
    if(valid != NULL) *valid = true;
    return clampf_local(pressure_psi, 0.0f, 100.0f);
}

float ecu_coolant_flow_bv2000_from_hz(float frequency_hz, bool input_valid,
                                      bool *valid)
{
    if(valid != NULL) *valid = false;
    if(!input_valid || !isfinite(frequency_hz) || (frequency_hz < 0.0f))
    {
        return NAN;
    }
    const float flow_lpm = frequency_hz * 60.0f /
                           ECU_BV2000_PULSES_PER_LITER;
    if((flow_lpm < ECU_BV2000_MIN_LPM) ||
       (flow_lpm > ECU_BV2000_MAX_LPM))
    {
        return NAN;
    }
    if(valid != NULL) *valid = true;
    return flow_lpm;
}

void ecu_cooling_monitor_init(ecu_cooling_monitor_t *monitor)
{
    if(monitor == NULL) return;
    monitor->sample_count = 0u;
    monitor->bad_count = 0u;
    monitor->good_count = 0u;
    monitor->fault = false;
    monitor->fault_flags = 0u;
}

bool ecu_cooling_monitor_update(ecu_cooling_monitor_t *monitor,
                                const ecu_cooling_sample_t *sample)
{
    if((monitor == NULL) || (sample == NULL)) return true;
    if(monitor->sample_count != UINT16_MAX) monitor->sample_count++;

    uint16_t reasons = 0u;
    if(!sample->temp_in_valid) reasons |= ECU_COOLING_FAULT_TEMP_IN_SENSOR;
    if(!sample->temp_out_valid) reasons |= ECU_COOLING_FAULT_TEMP_OUT_SENSOR;
    if(!sample->pressure_valid) reasons |= ECU_COOLING_FAULT_PRESS_SENSOR;

    const bool past_startup = monitor->sample_count >=
                              ECU_COOLING_STARTUP_SAMPLES;
    const bool pump_expected = isfinite(sample->pump_command_pct) &&
                               (sample->pump_command_pct >= 20.0f);
    if(past_startup && pump_expected && !sample->flow_valid)
    {
        reasons |= ECU_COOLING_FAULT_FLOW_STALE;
    }
    if(sample->temp_in_valid && sample->temp_out_valid)
    {
        const float maximum_c = (sample->temp_in_c > sample->temp_out_c) ?
                                sample->temp_in_c : sample->temp_out_c;
        if(maximum_c >= ECU_COOLING_MAX_FLUID_C)
        {
            reasons |= ECU_COOLING_FAULT_OVER_TEMP;
        }
        if(fabsf(sample->temp_in_c - sample->temp_out_c) >=
           ECU_COOLING_MAX_TEMP_DELTA_C)
        {
            reasons |= ECU_COOLING_FAULT_TEMP_DELTA;
        }
    }
    if(past_startup && pump_expected && sample->flow_valid &&
       (sample->flow_lpm < ECU_COOLING_MIN_FLOW_LPM))
    {
        reasons |= ECU_COOLING_FAULT_LOW_FLOW;
    }
    if(sample->pressure_valid &&
       (sample->pressure_psi > ECU_COOLING_MAX_PRESSURE_PSI))
    {
        reasons |= ECU_COOLING_FAULT_OVER_PRESSURE;
    }

    /* Over-temperature clears only after the measured value is below the
     * explicit recovery threshold. This avoids chatter at 85 C. */
    if(monitor->fault && ((monitor->fault_flags & ECU_COOLING_FAULT_OVER_TEMP) != 0u) &&
       sample->temp_in_valid && sample->temp_out_valid &&
       ((sample->temp_in_c > ECU_COOLING_CLEAR_FLUID_C) ||
        (sample->temp_out_c > ECU_COOLING_CLEAR_FLUID_C)))
    {
        reasons |= ECU_COOLING_FAULT_OVER_TEMP;
    }

    if(reasons != 0u)
    {
        monitor->good_count = 0u;
        if(monitor->bad_count < UINT8_MAX) monitor->bad_count++;
        monitor->fault_flags |= reasons;
        if(monitor->bad_count >= ECU_COOLING_TRIP_SAMPLES)
        {
            monitor->fault = true;
        }
    }
    else
    {
        monitor->bad_count = 0u;
        if(monitor->good_count < UINT8_MAX) monitor->good_count++;
        if(monitor->good_count >= ECU_COOLING_CLEAR_SAMPLES)
        {
            monitor->fault = false;
            monitor->fault_flags = 0u;
        }
    }
    return monitor->fault;
}

ecu_coolant_pump_command_t ecu_coolant_pump_command(float requested_pct,
                                                     bool failsafe_max,
                                                     bool manual)
{
    ecu_coolant_pump_command_t command = {0};
    command.requested_pct = requested_pct;
    if(failsafe_max || !isfinite(requested_pct))
    {
        /* PB8 drives an inverting low-side MOSFET. Gate low releases S, which
         * deliberately invokes the PCE-XL no-valid-PWM/full-speed fallback. */
        command.requested_pct = 100.0f;
        command.pump_s_duty_pct = 100.0f;
        command.mcu_gate_duty_pct = 0.0f;
        command.flags = ECU_COOLING_PUMP_FLAG_FAILSAFE_MAX;
    }
    else
    {
        requested_pct = clampf_local(requested_pct, 0.0f, 100.0f);
        command.requested_pct = requested_pct;
        command.pump_s_duty_pct = 12.0f + (0.81f * requested_pct);
        command.mcu_gate_duty_pct = 100.0f - command.pump_s_duty_pct;
        command.flags = ECU_COOLING_PUMP_FLAG_VALID_PWM;
    }
    if(manual) command.flags |= ECU_COOLING_PUMP_FLAG_MANUAL;
    return command;
}
