/*
 * ecu_config.h
 *
 * Safety-critical build contract for the DER26 ECU.  Keep hardware semantics
 * here instead of relying on generated CubeMX net names.
 */

#ifndef __ECU_CONFIG_H_
#define __ECU_CONFIG_H_

/*
 * 0: bench profile.  Torque requests and the two CM200 hardware-enable outputs
 *    remain inhibited while all sensing, CAN reception, CLI, and diagnostics run.
 * 1: vehicle profile. Requires all release evidence below.
 * 2: test-day profile. Real sensing/CAN/logging/cooling execute, but propulsion
 *    outputs remain inhibited exactly like bench.
 */
#ifndef ECU_BUILD_PROFILE
#define ECU_BUILD_PROFILE 0
#endif

#define ECU_BUILD_PROFILE_BENCH   0
#define ECU_BUILD_PROFILE_VEHICLE 1
#define ECU_BUILD_PROFILE_TESTDAY 2

/* Immutable source-package provenance for this reviewed TESTDAY drop.
 * This is an archive identity, not a claim that a Git object was available in
 * the review environment. */
#ifndef ECU_BUILD_SOURCE_REVISION
#define ECU_BUILD_SOURCE_REVISION "DER26-ECU-v2.10.7-SAFETY2-20260827"
#endif
#define ECU_CAN_LOGGER_SCHEMA_REVISION "LOGGER3"
#define ECU_BUILD_CONFIG_FINGERPRINT 0xEC021007u

#if ((ECU_BUILD_PROFILE != ECU_BUILD_PROFILE_BENCH) && \
     (ECU_BUILD_PROFILE != ECU_BUILD_PROFILE_VEHICLE) && \
     (ECU_BUILD_PROFILE != ECU_BUILD_PROFILE_TESTDAY))
#error "ECU_BUILD_PROFILE must be 0 (bench), 1 (vehicle), or 2 (testday)"
#endif

#if ECU_BUILD_PROFILE == ECU_BUILD_PROFILE_VEHICLE
#define ECU_BUILD_PROFILE_NAME "vehicle"
#elif ECU_BUILD_PROFILE == ECU_BUILD_PROFILE_TESTDAY
#define ECU_BUILD_PROFILE_NAME "testday"
#else
#define ECU_BUILD_PROFILE_NAME "bench"
#endif

/*
 * The original BSPD schematic exported a nominal 12 V, active-high 'BSPD Ok'
 * signal. The 2026-08-25 hardware correction adds a protected fail-low 3.3 V
 * interface before PE13; this reviewed package records that hardware item as
 * resolved.
 */
#ifndef ECU_BSPD_INTERFACE_3V3_VALIDATED
#define ECU_BSPD_INTERFACE_3V3_VALIDATED 1
#endif

#if ((ECU_BSPD_INTERFACE_3V3_VALIDATED != 0) && \
     (ECU_BSPD_INTERFACE_3V3_VALIDATED != 1))
#error "ECU_BSPD_INTERFACE_3V3_VALIDATED must be 0 or 1"
#endif

#if ((ECU_BUILD_PROFILE == ECU_BUILD_PROFILE_VEHICLE) && \
     (ECU_BSPD_INTERFACE_3V3_VALIDATED != 1))
#error "Vehicle profile requires a validated 12 V BSPD-OK to 3.3 V PE13 interface"
#endif

/* The torque gate depends on these CM200 EEPROM/CAN settings being
 * measured on the real controller: standard 11-bit offset 0x0A0, required
 * A5/A7/AA/AB/AC/B1 broadcasts active at the documented rates, CAN torque
 * mode, and rolling-counter checking enabled. The fixed 333 Hz 0x0B0 high-
 * speed stream and other unused 100 Hz broadcasts must remain disabled unless
 * the whole-vehicle DER26-CAN-V4 arbitration/load test explicitly budgets them.
 * Vehicle target is 500 kbps; the safety/authority architecture must remain
 * correct at 250 kbps with detail telemetry allowed to degrade/supersede. */
#ifndef ECU_CM200_CAN_CONTRACT_VALIDATED
#define ECU_CM200_CAN_CONTRACT_VALIDATED 0
#endif

#if ((ECU_CM200_CAN_CONTRACT_VALIDATED != 0) && \
     (ECU_CM200_CAN_CONTRACT_VALIDATED != 1))
#error "ECU_CM200_CAN_CONTRACT_VALIDATED must be 0 or 1"
#endif

#if ((ECU_BUILD_PROFILE == ECU_BUILD_PROFILE_VEHICLE) && \
     (ECU_CM200_CAN_CONTRACT_VALIDATED != 1))
#error "Vehicle profile requires validated CM200 broadcast IDs/rates/mode/counter settings"
#endif


/* The deterministic torque-to-pack-current clamp implementation and contract
 * tests are present. Vehicle use remains independently locked by the validation
 * evidence gate and by the deliberately invalid checked-in calibration. */
#ifdef ECU_AMS_POWER_CLAMP_IMPLEMENTED
#error "ECU_AMS_POWER_CLAMP_IMPLEMENTED is source controlled; do not define it externally"
#endif
#define ECU_AMS_POWER_CLAMP_IMPLEMENTED 1

#if ((ECU_AMS_POWER_CLAMP_IMPLEMENTED != 0) && \
     (ECU_AMS_POWER_CLAMP_IMPLEMENTED != 1))
#error "ECU_AMS_POWER_CLAMP_IMPLEMENTED must be 0 or 1"
#endif

/* Independent evidence acknowledgement.  This may be supplied by a release
 * build only after the source-owned implementation latch is enabled. */
#ifndef ECU_AMS_POWER_CLAMP_VALIDATED
#define ECU_AMS_POWER_CLAMP_VALIDATED 0
#endif

#if ((ECU_AMS_POWER_CLAMP_VALIDATED != 0) && \
     (ECU_AMS_POWER_CLAMP_VALIDATED != 1))
#error "ECU_AMS_POWER_CLAMP_VALIDATED must be 0 or 1"
#endif

#if ((ECU_BUILD_PROFILE == ECU_BUILD_PROFILE_VEHICLE) && \
     (ECU_AMS_POWER_CLAMP_IMPLEMENTED != 1))
#error "Vehicle profile requires the conservative AMS DCL/CCL torque clamp implementation"
#endif

#if ((ECU_BUILD_PROFILE == ECU_BUILD_PROFILE_VEHICLE) && \
     (ECU_AMS_POWER_CLAMP_VALIDATED != 1))
#error "Vehicle profile requires validation evidence for the conservative AMS DCL/CCL torque clamp"
#endif


/* Full-vehicle release evidence gates. Bench builds remain available with all
 * gates at zero. Each gate corresponds to an independently reviewable artifact
 * and may not be replaced by one generic release switch. */
#ifndef ECU_PINMAP_VALIDATED
#define ECU_PINMAP_VALIDATED 0
#endif
#ifndef ECU_APPS_CALIBRATION_VALIDATED
#define ECU_APPS_CALIBRATION_VALIDATED 0
#endif
#ifndef ECU_BSE_CALIBRATION_VALIDATED
#define ECU_BSE_CALIBRATION_VALIDATED 0
#endif
#ifndef ECU_DISCRETE_INPUTS_VALIDATED
#define ECU_DISCRETE_INPUTS_VALIDATED 0
#endif
#ifndef ECU_AMS_PROTOCOL_VALIDATED
#define ECU_AMS_PROTOCOL_VALIDATED 0
#endif
#ifndef ECU_CURRENT_MODEL_VALIDATED
#define ECU_CURRENT_MODEL_VALIDATED 0
#endif
#ifndef ECU_CURRENT_RESIDUAL_VALIDATED
#define ECU_CURRENT_RESIDUAL_VALIDATED 0
#endif
/* Residual-monitor measurement uncertainty is part of the vehicle release
 * evidence, not a magic test-only constant. Values are in 0.1 A so they remain
 * usable in preprocessor gates and are converted to amperes by canbus_task. */
#ifndef ECU_CURRENT_RESIDUAL_UNCERTAINTY_NEG_0P1A
#define ECU_CURRENT_RESIDUAL_UNCERTAINTY_NEG_0P1A 0u
#endif
#ifndef ECU_CURRENT_RESIDUAL_UNCERTAINTY_POS_0P1A
#define ECU_CURRENT_RESIDUAL_UNCERTAINTY_POS_0P1A 0u
#endif

#if (ECU_BUILD_PROFILE == ECU_BUILD_PROFILE_VEHICLE) && \
    ECU_CURRENT_RESIDUAL_VALIDATED && \
    ((ECU_CURRENT_RESIDUAL_UNCERTAINTY_NEG_0P1A == 0u) || \
     (ECU_CURRENT_RESIDUAL_UNCERTAINTY_POS_0P1A == 0u))
#error "Vehicle current-residual validation requires nonzero measured negative/positive current uncertainty"
#endif
#ifndef ECU_COOLING_VALIDATED
#define ECU_COOLING_VALIDATED 0
#endif

/* Source-owned implementation latch. Conversion, plausibility, debounced
 * fault policy and inverted PCE-XL PWM are implemented and host tested.
 * ECU_COOLING_VALIDATED remains a separate target-evidence gate for bath,
 * pressure, flow and assembled-loop validation. */
#ifdef ECU_COOLING_IMPLEMENTATION_COMPLETE
#error "ECU_COOLING_IMPLEMENTATION_COMPLETE is source controlled; do not define it externally"
#endif
#define ECU_COOLING_IMPLEMENTATION_COMPLETE 1
#ifndef ECU_RTOS_MEMORY_VALIDATED
#define ECU_RTOS_MEMORY_VALIDATED 0
#endif
#ifndef ECU_WCET_VALIDATED
#define ECU_WCET_VALIDATED 0
#endif
#ifndef ECU_CAN_LOAD_VALIDATED
#define ECU_CAN_LOAD_VALIDATED 0
#endif
#ifndef ECU_WATCHDOG_VALIDATED
#define ECU_WATCHDOG_VALIDATED 0
#endif
#ifndef ECU_SAFE_OUTPUTS_VALIDATED
#define ECU_SAFE_OUTPUTS_VALIDATED 0
#endif

#define ECU_FULL_RELEASE_EVIDENCE \
    (ECU_PINMAP_VALIDATED && ECU_APPS_CALIBRATION_VALIDATED && \
     ECU_BSE_CALIBRATION_VALIDATED && ECU_DISCRETE_INPUTS_VALIDATED && \
     ECU_AMS_PROTOCOL_VALIDATED && ECU_CURRENT_MODEL_VALIDATED && \
     ECU_CURRENT_RESIDUAL_VALIDATED && ECU_COOLING_VALIDATED && \
     ECU_RTOS_MEMORY_VALIDATED && ECU_WCET_VALIDATED && \
     ECU_CAN_LOAD_VALIDATED && ECU_WATCHDOG_VALIDATED && \
     ECU_SAFE_OUTPUTS_VALIDATED)

#if ((ECU_BUILD_PROFILE == ECU_BUILD_PROFILE_VEHICLE) && \
     !(ECU_FULL_RELEASE_EVIDENCE))
#error "Vehicle profile requires complete ECU pin/input/current/cooling/RTOS/WCET/CAN/watchdog release evidence"
#endif

#if ((ECU_BUILD_PROFILE == ECU_BUILD_PROFILE_VEHICLE) && \
     (ECU_COOLING_IMPLEMENTATION_COMPLETE != 1))
#error "Vehicle profile is locked: coolant temperature conversion and coolant fault policy are not implemented/calibrated"
#endif

/* Best-effort SD logger. It is deliberately non-gating and lower priority
 * than every sensing/control task. Bench images auto-start when a card is
 * available; vehicle release still requires separate logging/CAN-load proof. */
#ifndef ECU_DATA_LOGGER_ENABLE
#define ECU_DATA_LOGGER_ENABLE 1
#endif
#ifndef ECU_DATA_LOGGER_AUTOSTART
#define ECU_DATA_LOGGER_AUTOSTART \
    ((ECU_BUILD_PROFILE == ECU_BUILD_PROFILE_BENCH) || \
     (ECU_BUILD_PROFILE == ECU_BUILD_PROFILE_TESTDAY) || \
     ((ECU_BUILD_PROFILE == ECU_BUILD_PROFILE_VEHICLE) && ECU_CAN_LOAD_VALIDATED))
#endif
#ifndef ECU_DATA_LOGGER_DEFAULT_HZ
#define ECU_DATA_LOGGER_DEFAULT_HZ 10u
#endif
#if (ECU_DATA_LOGGER_DEFAULT_HZ < 1u) || (ECU_DATA_LOGGER_DEFAULT_HZ > 100u)
#error "ECU_DATA_LOGGER_DEFAULT_HZ must be 1..100 Hz"
#endif

#define ECU_OUTPUTS_INHIBITED (ECU_BUILD_PROFILE != ECU_BUILD_PROFILE_VEHICLE)
#define ECU_ENABLE_IWDG       (ECU_BUILD_PROFILE == ECU_BUILD_PROFILE_VEHICLE)

/* Approx. 2 s at the nominal 32 kHz LSI: /64, reload 999. */
#define ECU_IWDG_PRESCALER_REGISTER 4u
#define ECU_IWDG_RELOAD_REGISTER    999u

/* Discrete input semantics after the external interface circuitry. */
#define ECU_BSPD_OK_ACTIVE_HIGH       1
#define ECU_BMS_FAIL_ACTIVE_HIGH      1
#define ECU_IMD_FAIL_ACTIVE_HIGH      1
#define ECU_MTR_FAULT_ACTIVE_HIGH     1
#define ECU_MTR_OK_ACTIVE_LOW         1
#define ECU_TSAL_ACTIVE_HIGH          1
#define ECU_RTD_BUTTON_ACTIVE_LOW     1

/* Faults assert immediately; recovery requires 250 ms of healthy 100 Hz reads. */
#define ECU_DISCRETE_CLEAR_SAMPLES    25u

/* APPS and dual-brake plausibility are evaluated at 100 Hz. Using a 90 ms
 * observed duration guarantees a response no later than 100 ms after physical
 * onset, including one scheduler-period phase uncertainty. */
#define ECU_APPS_IMPLAUSIBILITY_LIMIT_MS 90u
#define ECU_BSE_IMPLAUSIBILITY_LIMIT_MS  90u

/* CM200 command contract from 0A-0163-04. */
#define ECU_CM200_FORWARD_DIRECTION   1u
#define ECU_CM200_ROLLING_COUNTER_MASK 0xF0u
#define ECU_CM200_CONTROL_MASK         0x0Fu

/* The APPS/command task runs at 100 Hz.  These limits correspond to 1000 Nm/s
 * rise and 2000 Nm/s fall; a safety fault still commands zero immediately. */
#define ECU_TORQUE_RISE_STEP_0P1NM     100u
#define ECU_TORQUE_FALL_STEP_0P1NM     200u
#define ECU_CM200_POWER_SEQUENCE_DELAY_MS 3000u

/* Diagnostic-only until measured on the assembled HV topology. */
#define ECU_AMS_CM200_VOLTAGE_TOLERANCE_0P1V 200u

/* Target WCET instrumentation. The hard threshold is below the 10 ms control
 * period so a late nonzero command is converted to zero before publication. */
#define ECU_TORQUE_CLAMP_SOFT_BUDGET_US 3000u
#define ECU_TORQUE_CLAMP_HARD_BUDGET_US 8000u
#define ECU_TORQUE_CLAMP_OVERRUN_TRIP_COUNT 2u

#if (ECU_TORQUE_CLAMP_SOFT_BUDGET_US >= ECU_TORQUE_CLAMP_HARD_BUDGET_US)
#error "Torque-clamp soft WCET budget must be below the hard budget"
#endif
#if (ECU_TORQUE_CLAMP_HARD_BUDGET_US >= 10000u)
#error "Torque-clamp hard WCET budget must remain below the 100 Hz period"
#endif

/* Keep the cooling pump at its safe bench command until calibration is closed. */
#define ECU_COOLANT_PUMP_DEFAULT_PERCENT 100u

#endif /* __ECU_CONFIG_H_ */
