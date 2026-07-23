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
 * 1: vehicle profile.  Requires the BPSD interface acknowledgement below.
 */
#ifndef ECU_BUILD_PROFILE
#define ECU_BUILD_PROFILE 0
#endif

#define ECU_BUILD_PROFILE_BENCH   0
#define ECU_BUILD_PROFILE_VEHICLE 1

#if ((ECU_BUILD_PROFILE != ECU_BUILD_PROFILE_BENCH) && \
     (ECU_BUILD_PROFILE != ECU_BUILD_PROFILE_VEHICLE))
#error "ECU_BUILD_PROFILE must be 0 (bench) or 1 (vehicle)"
#endif

/*
 * The BPSD schematic exports a nominal 12 V, active-high 'BSPD Ok' signal.
 * PE13 is a 3.3 V MCU input and the breakout schematic shows no level shifter.
 * The vehicle profile is therefore locked until a protected, fail-low 3.3 V
 * interface has been installed and electrically validated.
 */
#ifndef ECU_BSPD_INTERFACE_3V3_VALIDATED
#define ECU_BSPD_INTERFACE_3V3_VALIDATED 0
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
 * mode, and rolling-counter checking enabled. */
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


/* Power protocol v2 reception is integrated, but the numeric conservative
 * torque-to-DC-current/power clamp is not implemented yet.  This source-owned
 * latch deliberately cannot be enabled from a compiler command line: the
 * implementation commit must change it to 1 after the stall/low-speed-safe
 * clamp and its tests exist. */
#ifdef ECU_AMS_POWER_CLAMP_IMPLEMENTED
#error "ECU_AMS_POWER_CLAMP_IMPLEMENTED is source controlled; do not define it externally"
#endif
#define ECU_AMS_POWER_CLAMP_IMPLEMENTED 0

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

/* Keep the cooling pump at its safe bench command until calibration is closed. */
#define ECU_COOLANT_PUMP_DEFAULT_PERCENT 100u

#endif /* __ECU_CONFIG_H_ */
