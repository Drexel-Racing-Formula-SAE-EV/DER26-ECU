# DER26 ECU Firmware v2.3.0 Validation Report

Date: 2026-07-21

## Scope

This report covers the ECU safety-hardening update against the supplied ECU source,
AMS firmware reference, BPSD/MCU-breakout/shutdown schematics, and ECU hardware
documentation. The highest-risk focus was the BPSD interface and interpretation,
followed by AMS CAN supervision, torque-command safety, critical-output ownership,
sensor failure behavior, watchdog/fault handling, and bring-up diagnostics.

## Automated validation completed

The following checks passed from a clean host-test build:

- 17 focused unit tests.
- 8 host regression tests.
- 10 system software-in-the-loop tests.
- 50,000-cycle deterministic stress/fuzz run.
- AddressSanitizer and UndefinedBehaviorSanitizer runs of all host suites.
- Standalone UndefinedBehaviorSanitizer runs of all host suites.
- GCC static analysis with `-fanalyzer`.
- Strict warning-as-error syntax/type checks over the complete `Core/Src` tree for:
  - the inhibited bench profile; and
  - the acknowledged vehicle profile.
- Build-profile gates proving that:
  - the bench profile compiles;
  - an unacknowledged vehicle build is rejected; and
  - a vehicle build with the explicit BPSD interface acknowledgement compiles.
- Repository structure, hygiene, and CI shell-script syntax checks.

Commands used:

```sh
make -C host_tests CC=gcc clean
make -C host_tests CC=gcc ci
make -C host_tests CC=gcc asan
make -C host_tests CC=gcc ubsan
make -C host_tests CC=gcc stress
bash ci/scripts/check_core_host_syntax.sh
bash ci/scripts/check_project_structure.sh
bash -n ci/stm32/build_ecu_headless_gcc.sh \
  ci/scripts/check_core_host_syntax.sh \
  ci/scripts/check_project_structure.sh \
  ci/scripts/check_repo_hygiene.sh
```

## Safety behavior covered by tests and source checks

- BPSD healthy-high/fail-low interpretation and fail-closed disconnect behavior.
- Immediate BPSD fault assertion and delayed healthy recovery debounce.
- Vehicle-profile compile lock until the protected 12 V-to-3.3 V BPSD interface is
  explicitly validated.
- Independent freshness and validation of required AMS summary frames `0x680`,
  `0x681`, and `0x682`.
- AMS protocol, sequence, status, fault, range, and thermal validity checks.
- Signed little-endian CM200 torque encoding, rolling counter behavior, pre-enable
  disable frames, and safe disable direction handling.
- Sole-owner control of Firmware_OK, motor-enable, and inverter-enable outputs.
- Bench-profile output inhibition throughout the hardware and torque paths.
- Fail-low behavior from MCU exceptions, HAL errors, RTOS assertion/failure hooks,
  and watchdog supervision.
- ADC failure propagation, APPS raw plausibility checking, BSE fail-safe brake-light
  behavior, coolant-sensor invalidity handling, and conservative pump behavior.
- CAN controller error monitoring, bus-off recovery accounting, and diagnostic CLI.

## Not completed in this environment

`arm-none-eabi-gcc` and STM32CubeIDE were not available, so no STM32F767 target ELF,
map file, or flash image was produced here. The source was type-checked against the
project's host HAL/RTOS interfaces, but a clean target build in the team's exact
CubeIDE toolchain remains mandatory.

No physical ECU, BPSD, AMS, inverter, shutdown circuit, watchdog-reset, or CAN-bus
hardware test was performed. Before vehicle use, follow `Core/docs/ECU_HARDWARE_BRINGUP.md`
and `Core/docs/BSPD_INTERFACE_AND_TEST_PLAN.md`, validate the protected BPSD input
electrically, prove all shutdown paths with HV inhibited, and retain the independent
hardware shutdown chain.

## Required build selection

The supplied CubeIDE Debug and Release configurations remain deliberately inhibited:

```text
ECU_BUILD_PROFILE=0
```

Only after the documented BPSD interface and shutdown tests pass, create or select a
vehicle configuration containing both:

```text
ECU_BUILD_PROFILE=1
ECU_BSPD_INTERFACE_3V3_VALIDATED=1
```

The acknowledgement is a build gate, not evidence that the physical interface has
been validated.
