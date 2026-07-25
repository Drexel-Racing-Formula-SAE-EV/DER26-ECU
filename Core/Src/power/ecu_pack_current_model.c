#include "power/ecu_pack_current_model.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

static bool finite_interval(ecu_current_interval_t interval)
{
    return isfinite(interval.min_a) && isfinite(interval.max_a) &&
           (interval.min_a <= interval.max_a);
}

static bool finite_domain(float minimum, float maximum)
{
    return isfinite(minimum) && isfinite(maximum) && (minimum <= maximum);
}

static bool nonnegative_finite(float value)
{
    return isfinite(value) && (value >= 0.0f);
}

static bool transition_profile_valid(ecu_transition_profile_t profile)
{
    return (profile > ECU_TRANSITION_PROFILE_INVALID) &&
           (profile <= ECU_TRANSITION_PROFILE_COMPOSED);
}

static bool transition_direction_valid(ecu_transition_direction_t direction)
{
    return (direction > ECU_TRANSITION_DIRECTION_INVALID) &&
           (direction <= ECU_TRANSITION_DIRECTION_REVERSAL_FIRST_LEG);
}

uint32_t ecu_pack_current_calibration_crc32(
    const ecu_pack_current_calibration_t *cal)
{
    if(cal == NULL)
    {
        return 0u;
    }

    const uint8_t *bytes = (const uint8_t *)cal;
    const size_t length = offsetof(ecu_pack_current_calibration_t, crc32);
    uint32_t crc = 0xFFFFFFFFu;

    for(size_t i = 0u; i < length; ++i)
    {
        crc ^= bytes[i];
        for(uint8_t bit = 0u; bit < 8u; ++bit)
        {
            const uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1u) ^ (0xEDB88320u & mask);
        }
    }

    return crc ^ 0xFFFFFFFFu;
}

static bool steady_region_valid(const ecu_steady_operating_region_t *region)
{
    return (region != NULL) &&
           isfinite(region->speed_min_rpm) &&
           isfinite(region->speed_max_rpm) &&
           (region->speed_min_rpm < region->speed_max_rpm) &&
           isfinite(region->vdc_min_v) && isfinite(region->vdc_max_v) &&
           (region->vdc_min_v > 0.0f) &&
           (region->vdc_min_v < region->vdc_max_v) &&
           isfinite(region->inverter_temp_min_c) &&
           isfinite(region->inverter_temp_max_c) &&
           (region->inverter_temp_min_c < region->inverter_temp_max_c) &&
           isfinite(region->motor_temp_min_c) &&
           isfinite(region->motor_temp_max_c) &&
           (region->motor_temp_min_c < region->motor_temp_max_c) &&
           finite_interval(region->current_a);
}

static bool transition_region_valid(
    const ecu_transition_operating_region_t *region)
{
    return (region != NULL) &&
           isfinite(region->speed_min_rpm) &&
           isfinite(region->speed_max_rpm) &&
           (region->speed_min_rpm < region->speed_max_rpm) &&
           isfinite(region->vdc_min_v) && isfinite(region->vdc_max_v) &&
           (region->vdc_min_v > 0.0f) &&
           (region->vdc_min_v < region->vdc_max_v) &&
           isfinite(region->inverter_temp_min_c) &&
           isfinite(region->inverter_temp_max_c) &&
           (region->inverter_temp_min_c < region->inverter_temp_max_c) &&
           isfinite(region->motor_temp_min_c) &&
           isfinite(region->motor_temp_max_c) &&
           (region->motor_temp_min_c < region->motor_temp_max_c) &&
           finite_interval(region->absolute_pack_current_a) &&
           (region->maximum_settling_time_us > 0u);
}


typedef struct
{
    float speed_min_rpm;
    float speed_max_rpm;
    float vdc_min_v;
    float vdc_max_v;
    float inverter_temp_min_c;
    float inverter_temp_max_c;
    float motor_temp_min_c;
    float motor_temp_max_c;
} operating_domain_t;

#define ECU_MODEL_MAX_DOMAIN_BOUNDARIES \
    (2u + 2u * ECU_CURRENT_MODEL_MAX_OPERATING_REGIONS_PER_CELL)

static operating_domain_t calibration_domain(
    const ecu_pack_current_calibration_t *cal)
{
    return (operating_domain_t){
        .speed_min_rpm = cal->certified_speed_min_rpm,
        .speed_max_rpm = cal->certified_speed_max_rpm,
        .vdc_min_v = cal->certified_vdc_min_v,
        .vdc_max_v = cal->certified_vdc_max_v,
        .inverter_temp_min_c = cal->certified_inverter_temp_min_c,
        .inverter_temp_max_c = cal->certified_inverter_temp_max_c,
        .motor_temp_min_c = cal->certified_motor_temp_min_c,
        .motor_temp_max_c = cal->certified_motor_temp_max_c,
    };
}

static operating_domain_t steady_region_domain(
    const ecu_steady_operating_region_t *region)
{
    return (operating_domain_t){
        .speed_min_rpm = region->speed_min_rpm,
        .speed_max_rpm = region->speed_max_rpm,
        .vdc_min_v = region->vdc_min_v,
        .vdc_max_v = region->vdc_max_v,
        .inverter_temp_min_c = region->inverter_temp_min_c,
        .inverter_temp_max_c = region->inverter_temp_max_c,
        .motor_temp_min_c = region->motor_temp_min_c,
        .motor_temp_max_c = region->motor_temp_max_c,
    };
}

static operating_domain_t transition_region_domain(
    const ecu_transition_operating_region_t *region)
{
    return (operating_domain_t){
        .speed_min_rpm = region->speed_min_rpm,
        .speed_max_rpm = region->speed_max_rpm,
        .vdc_min_v = region->vdc_min_v,
        .vdc_max_v = region->vdc_max_v,
        .inverter_temp_min_c = region->inverter_temp_min_c,
        .inverter_temp_max_c = region->inverter_temp_max_c,
        .motor_temp_min_c = region->motor_temp_min_c,
        .motor_temp_max_c = region->motor_temp_max_c,
    };
}

static bool domain_valid(operating_domain_t domain)
{
    return isfinite(domain.speed_min_rpm) &&
           isfinite(domain.speed_max_rpm) &&
           (domain.speed_min_rpm < domain.speed_max_rpm) &&
           isfinite(domain.vdc_min_v) && isfinite(domain.vdc_max_v) &&
           (domain.vdc_min_v > 0.0f) &&
           (domain.vdc_min_v < domain.vdc_max_v) &&
           isfinite(domain.inverter_temp_min_c) &&
           isfinite(domain.inverter_temp_max_c) &&
           (domain.inverter_temp_min_c < domain.inverter_temp_max_c) &&
           isfinite(domain.motor_temp_min_c) &&
           isfinite(domain.motor_temp_max_c) &&
           (domain.motor_temp_min_c < domain.motor_temp_max_c);
}

static bool domain_contains_point(const operating_domain_t *domain,
                                  float speed,
                                  float vdc,
                                  float inverter_temp,
                                  float motor_temp)
{
    return (speed >= domain->speed_min_rpm) &&
           (speed <= domain->speed_max_rpm) &&
           (vdc >= domain->vdc_min_v) && (vdc <= domain->vdc_max_v) &&
           (inverter_temp >= domain->inverter_temp_min_c) &&
           (inverter_temp <= domain->inverter_temp_max_c) &&
           (motor_temp >= domain->motor_temp_min_c) &&
           (motor_temp <= domain->motor_temp_max_c);
}

static void add_unique_boundary(float *values, uint8_t *count, float value)
{
    for(uint8_t i = 0u; i < *count; ++i)
    {
        if(values[i] == value)
        {
            return;
        }
    }
    if(*count < ECU_MODEL_MAX_DOMAIN_BOUNDARIES)
    {
        values[*count] = value;
        (*count)++;
    }
}

static void sort_boundaries(float *values, uint8_t count)
{
    for(uint8_t i = 1u; i < count; ++i)
    {
        const float value = values[i];
        uint8_t j = i;
        while((j > 0u) && (values[j - 1u] > value))
        {
            values[j] = values[j - 1u];
            --j;
        }
        values[j] = value;
    }
}

static uint8_t build_boundaries(float global_min,
                                float global_max,
                                const operating_domain_t *regions,
                                uint8_t region_count,
                                unsigned dimension,
                                float *values)
{
    uint8_t count = 0u;
    add_unique_boundary(values, &count, global_min);
    add_unique_boundary(values, &count, global_max);
    for(uint8_t i = 0u; i < region_count; ++i)
    {
        float minimum = 0.0f;
        float maximum = 0.0f;
        switch(dimension)
        {
            case 0u:
                minimum = regions[i].speed_min_rpm;
                maximum = regions[i].speed_max_rpm;
                break;
            case 1u:
                minimum = regions[i].vdc_min_v;
                maximum = regions[i].vdc_max_v;
                break;
            case 2u:
                minimum = regions[i].inverter_temp_min_c;
                maximum = regions[i].inverter_temp_max_c;
                break;
            default:
                minimum = regions[i].motor_temp_min_c;
                maximum = regions[i].motor_temp_max_c;
                break;
        }
        if((maximum < global_min) || (minimum > global_max))
        {
            continue;
        }
        add_unique_boundary(values, &count, fmaxf(minimum, global_min));
        add_unique_boundary(values, &count, fminf(maximum, global_max));
    }
    sort_boundaries(values, count);
    return count;
}

static bool regions_cover_domain(const operating_domain_t *regions,
                                 uint8_t region_count,
                                 operating_domain_t global)
{
    if((regions == NULL) || (region_count == 0u) || !domain_valid(global))
    {
        return false;
    }

    for(uint8_t i = 0u; i < region_count; ++i)
    {
        if(!domain_valid(regions[i]))
        {
            return false;
        }
        if((regions[i].speed_min_rpm <= global.speed_min_rpm) &&
           (regions[i].speed_max_rpm >= global.speed_max_rpm) &&
           (regions[i].vdc_min_v <= global.vdc_min_v) &&
           (regions[i].vdc_max_v >= global.vdc_max_v) &&
           (regions[i].inverter_temp_min_c <= global.inverter_temp_min_c) &&
           (regions[i].inverter_temp_max_c >= global.inverter_temp_max_c) &&
           (regions[i].motor_temp_min_c <= global.motor_temp_min_c) &&
           (regions[i].motor_temp_max_c >= global.motor_temp_max_c))
        {
            return true;
        }
    }

    float speed[ECU_MODEL_MAX_DOMAIN_BOUNDARIES];
    float vdc[ECU_MODEL_MAX_DOMAIN_BOUNDARIES];
    float inverter_temp[ECU_MODEL_MAX_DOMAIN_BOUNDARIES];
    float motor_temp[ECU_MODEL_MAX_DOMAIN_BOUNDARIES];
    const uint8_t speed_count = build_boundaries(
        global.speed_min_rpm, global.speed_max_rpm,
        regions, region_count, 0u, speed);
    const uint8_t vdc_count = build_boundaries(
        global.vdc_min_v, global.vdc_max_v,
        regions, region_count, 1u, vdc);
    const uint8_t inverter_count = build_boundaries(
        global.inverter_temp_min_c, global.inverter_temp_max_c,
        regions, region_count, 2u, inverter_temp);
    const uint8_t motor_count = build_boundaries(
        global.motor_temp_min_c, global.motor_temp_max_c,
        regions, region_count, 3u, motor_temp);

    if((speed_count < 2u) || (vdc_count < 2u) ||
       (inverter_count < 2u) || (motor_count < 2u))
    {
        return false;
    }

    for(uint8_t si = 0u; si + 1u < speed_count; ++si)
    {
        const float s = 0.5f * (speed[si] + speed[si + 1u]);
        for(uint8_t vi = 0u; vi + 1u < vdc_count; ++vi)
        {
            const float v = 0.5f * (vdc[vi] + vdc[vi + 1u]);
            for(uint8_t ii = 0u; ii + 1u < inverter_count; ++ii)
            {
                const float it = 0.5f *
                    (inverter_temp[ii] + inverter_temp[ii + 1u]);
                for(uint8_t mi = 0u; mi + 1u < motor_count; ++mi)
                {
                    const float mt = 0.5f *
                        (motor_temp[mi] + motor_temp[mi + 1u]);
                    bool covered = false;
                    for(uint8_t region = 0u; region < region_count; ++region)
                    {
                        if(domain_contains_point(&regions[region], s, v, it, mt))
                        {
                            covered = true;
                            break;
                        }
                    }
                    if(!covered)
                    {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}


static bool transition_family_covers_span(
    const ecu_pack_current_calibration_t *cal,
    ecu_transition_profile_t profile,
    ecu_transition_direction_t direction,
    float required_span_nm)
{
    for(uint16_t i = 0u; i < cal->transition_cell_count; ++i)
    {
        const ecu_transition_current_cell_t *cell = &cal->transition_cells[i];
        if((cell->profile == profile) && (cell->direction == direction) &&
           (cell->span_max_nm >= required_span_nm))
        {
            return true;
        }
    }
    return false;
}

static bool same_transition_domain(
    const ecu_transition_operating_region_t *left,
    const ecu_transition_operating_region_t *right)
{
    return (left->speed_min_rpm == right->speed_min_rpm) &&
           (left->speed_max_rpm == right->speed_max_rpm) &&
           (left->vdc_min_v == right->vdc_min_v) &&
           (left->vdc_max_v == right->vdc_max_v) &&
           (left->inverter_temp_min_c == right->inverter_temp_min_c) &&
           (left->inverter_temp_max_c == right->inverter_temp_max_c) &&
           (left->motor_temp_min_c == right->motor_temp_min_c) &&
           (left->motor_temp_max_c == right->motor_temp_max_c);
}

bool ecu_pack_current_calibration_validate_full(
    const ecu_pack_current_calibration_t *cal)
{
    if((cal == NULL) || (cal->magic != ECU_CURRENT_MODEL_MAGIC) ||
       (cal->schema_version != ECU_CURRENT_MODEL_SCHEMA_VERSION) ||
       !cal->evidence_valid || (cal->crc32 == 0u) ||
       (cal->crc32 != ecu_pack_current_calibration_crc32(cal)) ||
       (cal->torque_axis_points < 2u) ||
       (cal->torque_axis_points > ECU_CURRENT_MODEL_MAX_TORQUE_AXIS_POINTS) ||
       (cal->steady_cell_count != (uint16_t)(cal->torque_axis_points - 1u)) ||
       (cal->steady_cell_count > ECU_CURRENT_MODEL_MAX_TORQUE_CELLS) ||
       (cal->transition_cell_count == 0u) ||
       (cal->transition_cell_count > ECU_CURRENT_MODEL_MAX_TRANSITION_CELLS) ||
       !domain_valid(calibration_domain(cal)) ||
       !finite_domain(cal->r2d_aux_current_min_a,
                      cal->r2d_aux_current_max_a) ||
       !nonnegative_finite(cal->numeric_margin_negative_a) ||
       !nonnegative_finite(cal->numeric_margin_positive_a) ||
       !nonnegative_finite(cal->torque_uncertainty_negative_nm) ||
       !nonnegative_finite(cal->torque_uncertainty_positive_nm) ||
       !nonnegative_finite(cal->speed_sensor_uncertainty_negative_rpm) ||
       !nonnegative_finite(cal->speed_sensor_uncertainty_positive_rpm) ||
       !nonnegative_finite(cal->maximum_acceleration_rpm_per_s) ||
       !nonnegative_finite(cal->maximum_deceleration_rpm_per_s) ||
       !nonnegative_finite(cal->vdc_uncertainty_negative_v) ||
       !nonnegative_finite(cal->vdc_uncertainty_positive_v) ||
       !nonnegative_finite(
           cal->inverter_temperature_uncertainty_negative_c) ||
       !nonnegative_finite(
           cal->inverter_temperature_uncertainty_positive_c) ||
       !nonnegative_finite(cal->motor_temperature_uncertainty_negative_c) ||
       !nonnegative_finite(cal->motor_temperature_uncertainty_positive_c) ||
       (cal->maximum_speed_age_us == 0u) ||
       (cal->maximum_vdc_age_us == 0u) ||
       (cal->maximum_inverter_temperature_age_us == 0u) ||
       (cal->maximum_motor_temperature_age_us == 0u) ||
       !nonnegative_finite(cal->zero_enter_nm) ||
       !nonnegative_finite(cal->zero_exit_nm) ||
       (cal->zero_exit_nm <= cal->zero_enter_nm) ||
       !nonnegative_finite(cal->tracking_band_nm) ||
       !nonnegative_finite(cal->maximum_microstep_nm) ||
       !nonnegative_finite(cal->maximum_settled_command_rate_nm_per_s) ||
       !nonnegative_finite(cal->maximum_anchor_deviation_nm) ||
       !nonnegative_finite(cal->maximum_cumulative_drift_nm) ||
       (cal->settled_tracking_time_us == 0u) ||
       (cal->steady_confirmation_samples == 0u) ||
       !nonnegative_finite(cal->microstep_margin_negative_a) ||
       !nonnegative_finite(cal->microstep_margin_positive_a) ||
       !finite_interval(cal->late_zero_transition_current_a) ||
       (cal->late_zero_maximum_settling_time_us == 0u) ||
       !isfinite(cal->low_margin_consumption_fraction) ||
       (cal->low_margin_consumption_fraction < 0.0f) ||
       (cal->low_margin_consumption_fraction > 1.0f) ||
       !nonnegative_finite(cal->minimum_discharge_headroom_a) ||
       !nonnegative_finite(cal->minimum_charge_headroom_a) ||
       !nonnegative_finite(cal->nonzero_torque_threshold_nm))
    {
        return false;
    }

    for(uint16_t i = 1u; i < cal->torque_axis_points; ++i)
    {
        if(!isfinite(cal->torque_axis_nm[i - 1u]) ||
           !isfinite(cal->torque_axis_nm[i]) ||
           (cal->torque_axis_nm[i] <= cal->torque_axis_nm[i - 1u]))
        {
            return false;
        }
    }

    for(uint16_t i = 0u; i < cal->steady_cell_count; ++i)
    {
        const ecu_steady_current_cell_t *cell = &cal->steady_cells[i];
        if((cell->torque_min_nm != cal->torque_axis_nm[i]) ||
           (cell->torque_max_nm != cal->torque_axis_nm[i + 1u]) ||
           (cell->region_count == 0u) ||
           (cell->region_count > ECU_CURRENT_MODEL_MAX_OPERATING_REGIONS_PER_CELL))
        {
            return false;
        }

        operating_domain_t domains[
            ECU_CURRENT_MODEL_MAX_OPERATING_REGIONS_PER_CELL];
        for(uint8_t region = 0u; region < cell->region_count; ++region)
        {
            if(!steady_region_valid(&cell->regions[region]))
            {
                return false;
            }
            domains[region] = steady_region_domain(&cell->regions[region]);
        }
        if(!regions_cover_domain(domains, cell->region_count,
                                 calibration_domain(cal)))
        {
            return false;
        }
    }

    for(uint16_t i = 0u; i < cal->transition_cell_count; ++i)
    {
        const ecu_transition_current_cell_t *cell = &cal->transition_cells[i];
        if(!transition_profile_valid(cell->profile) ||
           !transition_direction_valid(cell->direction) ||
           (!isfinite(cell->span_max_nm) || (cell->span_max_nm <= 0.0f)) ||
           (cell->region_count == 0u) ||
           (cell->region_count >
            ECU_CURRENT_MODEL_MAX_TRANSITION_REGIONS_PER_CELL))
        {
            return false;
        }

        operating_domain_t domains[
            ECU_CURRENT_MODEL_MAX_TRANSITION_REGIONS_PER_CELL];
        for(uint8_t region = 0u; region < cell->region_count; ++region)
        {
            if(!transition_region_valid(&cell->regions[region]))
            {
                return false;
            }
            domains[region] = transition_region_domain(&cell->regions[region]);
        }
        if(!regions_cover_domain(domains, cell->region_count,
                                 calibration_domain(cal)))
        {
            return false;
        }
    }

    const float required_span_nm = fmaxf(
        fabsf(cal->torque_axis_nm[0]),
        fabsf(cal->torque_axis_nm[cal->torque_axis_points - 1u]));
    static const struct
    {
        ecu_transition_profile_t profile;
        ecu_transition_direction_t direction;
    } required_families[] = {
        {ECU_TRANSITION_PROFILE_SLEW_LIMITED,
         ECU_TRANSITION_DIRECTION_FROM_ZERO},
        {ECU_TRANSITION_PROFILE_SLEW_LIMITED,
         ECU_TRANSITION_DIRECTION_SAME_SIGN_INCREASE},
        {ECU_TRANSITION_PROFILE_SLEW_LIMITED,
         ECU_TRANSITION_DIRECTION_SAME_SIGN_DECREASE},
        {ECU_TRANSITION_PROFILE_COMPOSED,
         ECU_TRANSITION_DIRECTION_FROM_ZERO},
        {ECU_TRANSITION_PROFILE_COMPOSED,
         ECU_TRANSITION_DIRECTION_SAME_SIGN_INCREASE},
        {ECU_TRANSITION_PROFILE_COMPOSED,
         ECU_TRANSITION_DIRECTION_SAME_SIGN_DECREASE},
        {ECU_TRANSITION_PROFILE_ZERO_ASSERT,
         ECU_TRANSITION_DIRECTION_TO_ZERO},
        {ECU_TRANSITION_PROFILE_REVERSAL_TO_ZERO,
         ECU_TRANSITION_DIRECTION_REVERSAL_FIRST_LEG},
        {ECU_TRANSITION_PROFILE_UNKNOWN_TO_ZERO,
         ECU_TRANSITION_DIRECTION_TO_ZERO},
    };
    for(size_t i = 0u;
        i < sizeof(required_families) / sizeof(required_families[0]);
        ++i)
    {
        if(!transition_family_covers_span(
               cal, required_families[i].profile,
               required_families[i].direction, required_span_nm))
        {
            return false;
        }
    }

    /* The static late-zero fallback is used when a newly invalid calibration
     * or commit-time safety event prevents a fresh transition lookup. It must
     * contain every certified zero/decay family after numerical widening. */
    for(uint16_t i = 0u; i < cal->transition_cell_count; ++i)
    {
        const ecu_transition_current_cell_t *cell = &cal->transition_cells[i];
        const bool zero_family =
            (cell->profile == ECU_TRANSITION_PROFILE_ZERO_ASSERT) ||
            (cell->profile == ECU_TRANSITION_PROFILE_REVERSAL_TO_ZERO) ||
            (cell->profile == ECU_TRANSITION_PROFILE_UNKNOWN_TO_ZERO);
        if(!zero_family)
        {
            continue;
        }
        for(uint8_t region = 0u; region < cell->region_count; ++region)
        {
            const ecu_transition_operating_region_t *r =
                &cell->regions[region];
            if((cal->late_zero_transition_current_a.min_a >
                (r->absolute_pack_current_a.min_a -
                 cal->numeric_margin_negative_a)) ||
               (cal->late_zero_transition_current_a.max_a <
                (r->absolute_pack_current_a.max_a +
                 cal->numeric_margin_positive_a)) ||
               (cal->late_zero_maximum_settling_time_us <
                r->maximum_settling_time_us))
            {
                return false;
            }
        }
    }

    /* A larger span for the same physical profile/direction and operating
     * domain must contain every smaller-span envelope. This boot-only O(N^2)
     * check prevents a refinement search from depending on a non-monotonic
     * transition artifact. */
    for(uint16_t small_index = 0u;
        small_index < cal->transition_cell_count;
        ++small_index)
    {
        const ecu_transition_current_cell_t *small =
            &cal->transition_cells[small_index];
        for(uint16_t large_index = 0u;
            large_index < cal->transition_cell_count;
            ++large_index)
        {
            const ecu_transition_current_cell_t *large =
                &cal->transition_cells[large_index];
            if((large->profile != small->profile) ||
               (large->direction != small->direction) ||
               (large->span_max_nm <= small->span_max_nm))
            {
                continue;
            }

            for(uint8_t small_region = 0u;
                small_region < small->region_count;
                ++small_region)
            {
                const ecu_transition_operating_region_t *sr =
                    &small->regions[small_region];
                bool matching_large_region = false;
                for(uint8_t large_region = 0u;
                    large_region < large->region_count;
                    ++large_region)
                {
                    const ecu_transition_operating_region_t *lr =
                        &large->regions[large_region];
                    if(!same_transition_domain(sr, lr))
                    {
                        continue;
                    }
                    matching_large_region = true;
                    if((lr->absolute_pack_current_a.min_a >
                        sr->absolute_pack_current_a.min_a) ||
                       (lr->absolute_pack_current_a.max_a <
                        sr->absolute_pack_current_a.max_a) ||
                       (lr->maximum_settling_time_us <
                        sr->maximum_settling_time_us))
                    {
                        return false;
                    }
                    break;
                }
                if(!matching_large_region)
                {
                    return false;
                }
            }
        }
    }

    return true;
}

bool ecu_pack_current_calibration_qualify(
    const ecu_pack_current_calibration_t *cal,
    uint32_t generation,
    ecu_pack_current_calibration_runtime_t *runtime)
{
    if(runtime == NULL)
    {
        return false;
    }

    memset(runtime, 0, sizeof(*runtime));
    if(!ecu_pack_current_calibration_validate_full(cal))
    {
        return false;
    }

    runtime->calibration = cal;
    runtime->qualified_crc32 = cal->crc32;
    runtime->generation = generation;
    runtime->qualified = true;
    return true;
}

bool ecu_pack_current_calibration_runtime_valid(
    const ecu_pack_current_calibration_runtime_t *runtime)
{
    const ecu_pack_current_calibration_t *cal =
        (runtime != NULL) ? runtime->calibration : NULL;
    return (runtime != NULL) && runtime->qualified && (cal != NULL) &&
           (runtime->qualified_crc32 != 0u) &&
           (runtime->qualified_crc32 == cal->crc32) &&
           (cal->magic == ECU_CURRENT_MODEL_MAGIC) &&
           (cal->schema_version == ECU_CURRENT_MODEL_SCHEMA_VERSION) &&
           cal->evidence_valid &&
           (cal->torque_axis_points >= 2u) &&
           (cal->torque_axis_points <= ECU_CURRENT_MODEL_MAX_TORQUE_AXIS_POINTS) &&
           (cal->steady_cell_count == (uint16_t)(cal->torque_axis_points - 1u)) &&
           (cal->steady_cell_count <= ECU_CURRENT_MODEL_MAX_TORQUE_CELLS) &&
           (cal->transition_cell_count > 0u) &&
           (cal->transition_cell_count <= ECU_CURRENT_MODEL_MAX_TRANSITION_CELLS);
}

typedef struct
{
    float torque_min_nm;
    float torque_max_nm;
    float speed_min_rpm;
    float speed_max_rpm;
    float vdc_min_v;
    float vdc_max_v;
    float inverter_temp_min_c;
    float inverter_temp_max_c;
    float motor_temp_min_c;
    float motor_temp_max_c;
} model_uncertainty_box_t;

static bool build_uncertainty_box(const ecu_steady_current_input_t *input,
                                  const ecu_pack_current_calibration_t *cal,
                                  model_uncertainty_box_t *box)
{
    if((input == NULL) || (cal == NULL) || (box == NULL) ||
       !isfinite(input->raw_torque_nm) ||
       !isfinite(input->motor_speed_rpm) ||
       !isfinite(input->dc_bus_voltage_v) ||
       !isfinite(input->inverter_temp_c) ||
       !isfinite(input->motor_temp_c) ||
       (input->motor_speed_age_us > cal->maximum_speed_age_us) ||
       (input->dc_bus_voltage_age_us > cal->maximum_vdc_age_us) ||
       (input->inverter_temp_age_us >
        cal->maximum_inverter_temperature_age_us) ||
       (input->motor_temp_age_us > cal->maximum_motor_temperature_age_us))
    {
        return false;
    }

    const float speed_age_s = (float)input->motor_speed_age_us * 1.0e-6f;
    box->torque_min_nm = input->raw_torque_nm -
                         cal->torque_uncertainty_negative_nm;
    box->torque_max_nm = input->raw_torque_nm +
                         cal->torque_uncertainty_positive_nm;
    box->speed_min_rpm = input->motor_speed_rpm -
                         cal->speed_sensor_uncertainty_negative_rpm -
                         (cal->maximum_deceleration_rpm_per_s * speed_age_s);
    box->speed_max_rpm = input->motor_speed_rpm +
                         cal->speed_sensor_uncertainty_positive_rpm +
                         (cal->maximum_acceleration_rpm_per_s * speed_age_s);
    box->vdc_min_v = input->dc_bus_voltage_v -
                     cal->vdc_uncertainty_negative_v;
    box->vdc_max_v = input->dc_bus_voltage_v +
                     cal->vdc_uncertainty_positive_v;
    box->inverter_temp_min_c = input->inverter_temp_c -
        cal->inverter_temperature_uncertainty_negative_c;
    box->inverter_temp_max_c = input->inverter_temp_c +
        cal->inverter_temperature_uncertainty_positive_c;
    box->motor_temp_min_c = input->motor_temp_c -
        cal->motor_temperature_uncertainty_negative_c;
    box->motor_temp_max_c = input->motor_temp_c +
        cal->motor_temperature_uncertainty_positive_c;

    const operating_domain_t domain = calibration_domain(cal);
    return finite_domain(box->torque_min_nm, box->torque_max_nm) &&
           (box->torque_min_nm >= cal->torque_axis_nm[0]) &&
           (box->torque_max_nm <=
            cal->torque_axis_nm[cal->torque_axis_points - 1u]) &&
           finite_domain(box->speed_min_rpm, box->speed_max_rpm) &&
           (box->speed_min_rpm >= domain.speed_min_rpm) &&
           (box->speed_max_rpm <= domain.speed_max_rpm) &&
           finite_domain(box->vdc_min_v, box->vdc_max_v) &&
           (box->vdc_min_v >= domain.vdc_min_v) &&
           (box->vdc_max_v <= domain.vdc_max_v) &&
           finite_domain(box->inverter_temp_min_c,
                         box->inverter_temp_max_c) &&
           (box->inverter_temp_min_c >= domain.inverter_temp_min_c) &&
           (box->inverter_temp_max_c <= domain.inverter_temp_max_c) &&
           finite_domain(box->motor_temp_min_c, box->motor_temp_max_c) &&
           (box->motor_temp_min_c >= domain.motor_temp_min_c) &&
           (box->motor_temp_max_c <= domain.motor_temp_max_c);
}

static bool intervals_overlap(float left_min, float left_max,
                              float right_min, float right_max)
{
    return (left_max >= right_min) && (right_max >= left_min);
}

static bool steady_region_intersects(const model_uncertainty_box_t *box,
                                     const ecu_steady_operating_region_t *region)
{
    return intervals_overlap(box->speed_min_rpm, box->speed_max_rpm,
                             region->speed_min_rpm, region->speed_max_rpm) &&
           intervals_overlap(box->vdc_min_v, box->vdc_max_v,
                             region->vdc_min_v, region->vdc_max_v) &&
           intervals_overlap(box->inverter_temp_min_c,
                             box->inverter_temp_max_c,
                             region->inverter_temp_min_c,
                             region->inverter_temp_max_c) &&
           intervals_overlap(box->motor_temp_min_c, box->motor_temp_max_c,
                             region->motor_temp_min_c,
                             region->motor_temp_max_c);
}

static void interval_union_in_place(ecu_current_interval_t *aggregate,
                                    ecu_current_interval_t value,
                                    bool *initialized)
{
    if(!*initialized)
    {
        *aggregate = value;
        *initialized = true;
        return;
    }
    if(value.min_a < aggregate->min_a)
    {
        aggregate->min_a = value.min_a;
    }
    if(value.max_a > aggregate->max_a)
    {
        aggregate->max_a = value.max_a;
    }
}

static ecu_current_model_status_t evaluate_steady_cells(
    const uint16_t *cell_indices,
    uint16_t cell_count,
    const ecu_steady_current_input_t *input,
    const ecu_pack_current_calibration_runtime_t *runtime,
    ecu_steady_current_output_t *output)
{
    const ecu_pack_current_calibration_t *cal = runtime->calibration;
    model_uncertainty_box_t box;
    if(!build_uncertainty_box(input, cal, &box))
    {
        output->status = ECU_CURRENT_MODEL_OUT_OF_DOMAIN;
        return output->status;
    }

    bool initialized = false;
    ecu_current_interval_t aggregate = {FLT_MAX, -FLT_MAX};
    uint16_t regions_evaluated = 0u;

    for(uint16_t cell_position = 0u;
        cell_position < cell_count;
        ++cell_position)
    {
        const uint16_t cell_index = cell_indices[cell_position];
        if(cell_index >= cal->steady_cell_count)
        {
            output->status = ECU_CURRENT_MODEL_INVALID;
            return output->status;
        }

        const ecu_steady_current_cell_t *cell = &cal->steady_cells[cell_index];
        bool cell_region_found = false;
        for(uint8_t region_index = 0u;
            region_index < cell->region_count;
            ++region_index)
        {
            const ecu_steady_operating_region_t *region =
                &cell->regions[region_index];
            if(!steady_region_intersects(&box, region))
            {
                continue;
            }
            if(regions_evaluated >= ECU_CURRENT_MODEL_MAX_TOTAL_REGIONS_PER_POINT)
            {
                output->status = ECU_CURRENT_MODEL_REGION_OVERFLOW;
                return output->status;
            }
            interval_union_in_place(&aggregate, region->current_a, &initialized);
            regions_evaluated++;
            cell_region_found = true;
        }

        if(!cell_region_found)
        {
            output->status = ECU_CURRENT_MODEL_OUT_OF_DOMAIN;
            return output->status;
        }
    }

    if(!initialized)
    {
        output->status = ECU_CURRENT_MODEL_OUT_OF_DOMAIN;
        return output->status;
    }

    aggregate.min_a += cal->r2d_aux_current_min_a -
                       cal->numeric_margin_negative_a;
    aggregate.max_a += cal->r2d_aux_current_max_a +
                       cal->numeric_margin_positive_a;
    if(input->apply_microstep_margin)
    {
        aggregate.min_a -= cal->microstep_margin_negative_a;
        aggregate.max_a += cal->microstep_margin_positive_a;
    }

    output->pack_current_a = aggregate;
    output->torque_cells_evaluated = cell_count;
    output->regions_evaluated = regions_evaluated;
    output->low_speed_or_zero_crossing =
        (box.speed_min_rpm <= 0.0f) && (box.speed_max_rpm >= 0.0f);
    output->status = finite_interval(aggregate) ?
        ECU_CURRENT_MODEL_OK : ECU_CURRENT_MODEL_NUMERIC_FAULT;
    output->output_valid = (output->status == ECU_CURRENT_MODEL_OK);
    return output->status;
}

ecu_current_model_status_t ecu_pack_current_evaluate_steady(
    const ecu_steady_current_input_t *input,
    const ecu_pack_current_calibration_runtime_t *runtime,
    ecu_steady_current_output_t *output)
{
    if(output != NULL)
    {
        memset(output, 0, sizeof(*output));
        output->status = ECU_CURRENT_MODEL_INVALID;
    }
    if((input == NULL) || (output == NULL))
    {
        return ECU_CURRENT_MODEL_INVALID;
    }
    if(!ecu_pack_current_calibration_runtime_valid(runtime))
    {
        output->status = ECU_CURRENT_MODEL_UNCALIBRATED;
        return output->status;
    }

    const ecu_pack_current_calibration_t *cal = runtime->calibration;
    model_uncertainty_box_t box;
    if(!build_uncertainty_box(input, cal, &box))
    {
        output->status = ECU_CURRENT_MODEL_OUT_OF_DOMAIN;
        return output->status;
    }

    uint16_t cell_indices[ECU_CURRENT_MODEL_MAX_TORQUE_REGIONS_PER_POINT];
    uint16_t cell_count = 0u;
    for(uint16_t index = 0u; index < cal->steady_cell_count; ++index)
    {
        const ecu_steady_current_cell_t *cell = &cal->steady_cells[index];
        if(!intervals_overlap(box.torque_min_nm, box.torque_max_nm,
                              cell->torque_min_nm, cell->torque_max_nm))
        {
            continue;
        }
        if(cell_count >= ECU_CURRENT_MODEL_MAX_TORQUE_REGIONS_PER_POINT)
        {
            output->status = ECU_CURRENT_MODEL_REGION_OVERFLOW;
            return output->status;
        }
        cell_indices[cell_count++] = index;
    }

    if(cell_count == 0u)
    {
        output->status = ECU_CURRENT_MODEL_OUT_OF_DOMAIN;
        return output->status;
    }

    return evaluate_steady_cells(cell_indices, cell_count, input, runtime, output);
}

ecu_current_model_status_t ecu_pack_current_evaluate_steady_cell(
    uint16_t torque_cell_index,
    const ecu_steady_current_input_t *input,
    const ecu_pack_current_calibration_runtime_t *runtime,
    ecu_steady_current_output_t *output)
{
    if(output != NULL)
    {
        memset(output, 0, sizeof(*output));
        output->status = ECU_CURRENT_MODEL_INVALID;
    }
    if((input == NULL) || (output == NULL))
    {
        return ECU_CURRENT_MODEL_INVALID;
    }
    if(!ecu_pack_current_calibration_runtime_valid(runtime))
    {
        output->status = ECU_CURRENT_MODEL_UNCALIBRATED;
        return output->status;
    }
    if(torque_cell_index >= runtime->calibration->steady_cell_count)
    {
        output->status = ECU_CURRENT_MODEL_OUT_OF_DOMAIN;
        return output->status;
    }

    const uint16_t index = torque_cell_index;
    return evaluate_steady_cells(&index, 1u, input, runtime, output);
}

static bool transition_region_intersects(
    const model_uncertainty_box_t *box,
    const ecu_transition_operating_region_t *region)
{
    return intervals_overlap(box->speed_min_rpm, box->speed_max_rpm,
                             region->speed_min_rpm, region->speed_max_rpm) &&
           intervals_overlap(box->vdc_min_v, box->vdc_max_v,
                             region->vdc_min_v, region->vdc_max_v) &&
           intervals_overlap(box->inverter_temp_min_c,
                             box->inverter_temp_max_c,
                             region->inverter_temp_min_c,
                             region->inverter_temp_max_c) &&
           intervals_overlap(box->motor_temp_min_c, box->motor_temp_max_c,
                             region->motor_temp_min_c,
                             region->motor_temp_max_c);
}

ecu_current_model_status_t ecu_pack_current_evaluate_transition(
    const ecu_transition_current_input_t *input,
    const ecu_pack_current_calibration_runtime_t *runtime,
    ecu_transition_current_output_t *output)
{
    if(output != NULL)
    {
        memset(output, 0, sizeof(*output));
        output->status = ECU_CURRENT_MODEL_INVALID;
    }
    if((input == NULL) || (output == NULL) ||
       !isfinite(input->settled_anchor_torque_nm) ||
       !isfinite(input->minimum_raw_commanded_torque_nm) ||
       !isfinite(input->maximum_raw_commanded_torque_nm) ||
       !isfinite(input->latest_raw_target_torque_nm) ||
       !transition_profile_valid(input->profile) ||
       !transition_direction_valid(input->direction))
    {
        return ECU_CURRENT_MODEL_INVALID;
    }
    if(!ecu_pack_current_calibration_runtime_valid(runtime))
    {
        output->status = ECU_CURRENT_MODEL_UNCALIBRATED;
        return output->status;
    }

    const ecu_pack_current_calibration_t *cal = runtime->calibration;
    ecu_steady_current_input_t operating_input = {
        .raw_torque_nm = input->latest_raw_target_torque_nm,
        .motor_speed_rpm = input->motor_speed_rpm,
        .dc_bus_voltage_v = input->dc_bus_voltage_v,
        .inverter_temp_c = input->inverter_temp_c,
        .motor_temp_c = input->motor_temp_c,
        .motor_speed_age_us = input->motor_speed_age_us,
        .dc_bus_voltage_age_us = input->dc_bus_voltage_age_us,
        .inverter_temp_age_us = input->inverter_temp_age_us,
        .motor_temp_age_us = input->motor_temp_age_us,
        .apply_microstep_margin = false
    };
    model_uncertainty_box_t box;
    if(!build_uncertainty_box(&operating_input, cal, &box))
    {
        output->status = ECU_CURRENT_MODEL_OUT_OF_DOMAIN;
        return output->status;
    }

    const float span = input->maximum_raw_commanded_torque_nm -
                       input->minimum_raw_commanded_torque_nm;
    if(!isfinite(span) || (span < 0.0f))
    {
        output->status = ECU_CURRENT_MODEL_NUMERIC_FAULT;
        return output->status;
    }

    float selected_span_max = FLT_MAX;
    for(uint16_t index = 0u; index < cal->transition_cell_count; ++index)
    {
        const ecu_transition_current_cell_t *cell =
            &cal->transition_cells[index];
        if((cell->profile == input->profile) &&
           (cell->direction == input->direction) &&
           (span <= cell->span_max_nm) &&
           (cell->span_max_nm < selected_span_max))
        {
            selected_span_max = cell->span_max_nm;
        }
    }

    if(selected_span_max == FLT_MAX)
    {
        output->status = ECU_CURRENT_MODEL_OUT_OF_DOMAIN;
        return output->status;
    }

    bool initialized = false;
    ecu_current_interval_t aggregate = {FLT_MAX, -FLT_MAX};
    uint32_t maximum_settling_time_us = 0u;
    uint16_t regions_evaluated = 0u;

    for(uint16_t index = 0u; index < cal->transition_cell_count; ++index)
    {
        const ecu_transition_current_cell_t *cell =
            &cal->transition_cells[index];
        if((cell->profile != input->profile) ||
           (cell->direction != input->direction) ||
           (cell->span_max_nm != selected_span_max))
        {
            continue;
        }

        for(uint8_t region_index = 0u;
            region_index < cell->region_count;
            ++region_index)
        {
            const ecu_transition_operating_region_t *region =
                &cell->regions[region_index];
            if(!transition_region_intersects(&box, region))
            {
                continue;
            }
            if(regions_evaluated >= ECU_CURRENT_MODEL_MAX_TOTAL_REGIONS_PER_POINT)
            {
                output->status = ECU_CURRENT_MODEL_REGION_OVERFLOW;
                return output->status;
            }
            interval_union_in_place(&aggregate,
                                    region->absolute_pack_current_a,
                                    &initialized);
            if(region->maximum_settling_time_us > maximum_settling_time_us)
            {
                maximum_settling_time_us = region->maximum_settling_time_us;
            }
            regions_evaluated++;
        }
    }

    if(!initialized)
    {
        output->status = ECU_CURRENT_MODEL_OUT_OF_DOMAIN;
        return output->status;
    }

    aggregate.min_a -= cal->numeric_margin_negative_a;
    aggregate.max_a += cal->numeric_margin_positive_a;
    output->absolute_pack_current_a = aggregate;
    output->maximum_settling_time_us = maximum_settling_time_us;
    output->regions_evaluated = regions_evaluated;
    output->status = finite_interval(aggregate) ?
        ECU_CURRENT_MODEL_OK : ECU_CURRENT_MODEL_NUMERIC_FAULT;
    output->output_valid = (output->status == ECU_CURRENT_MODEL_OK);
    return output->status;
}
