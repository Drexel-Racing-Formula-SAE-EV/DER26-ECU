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

#if ((ECU_BUILD_PROFILE == ECU_BUILD_PROFILE_VEHICLE) && \
     (ECU_BSPD_INTERFACE_3V3_VALIDATED != 1))
#error "Vehicle profile requires a validated 12 V BSPD-OK to 3.3 V PE13 interface"
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

/* Faults assert immediately; recovery requires this many healthy 20 Hz reads. */
#define ECU_DISCRETE_CLEAR_SAMPLES    5u

/* CM200 command contract from 0A-0163-04. */
#define ECU_CM200_FORWARD_DIRECTION   1u
#define ECU_CM200_ROLLING_COUNTER_MASK 0xF0u
#define ECU_CM200_CONTROL_MASK         0x0Fu

/* Keep the cooling pump at its safe bench command until calibration is closed. */
#define ECU_COOLANT_PUMP_DEFAULT_PERCENT 100u

#endif /* __ECU_CONFIG_H_ */
