# DER26 ECU Firmware v2.4.0 Validation Report

Date: 2026-07-21

## Scope

This pass reviewed and changed the ECU firmware against the v2.3 safety release, the current AMS compact CAN contract, Cascadia Motion CM software manual `0A-0163-04`, and the supplied ECU/MCU-breakout/BPSD/shutdown/RTM hardware references. The highest-risk additions were CM200 broadcast supervision and command correlation, followed by stale-command prevention, RTD correction, protocol-specific CAN invalidation, AMS plausibility, and staged inverter power/feedback behavior.

## Automated validation

The final source passed:

- 21 focused unit tests.
- 8 host regression tests.
- 14 system SIL/fault-injection tests.
- 43 named tests total.
- Extended deterministic stress with 50,000 AMS iterations and 50,000 CM200 random-frame iterations.
- AddressSanitizer plus UndefinedBehaviorSanitizer on all three suites.
- Standalone UndefinedBehaviorSanitizer on all three suites.
- GCC `-fanalyzer` on every host-testable parser/safety suite.
- GCC `-fanalyzer` over the complete 34-file `Core/Src` application tree in both the bench and fully acknowledged vehicle profiles.
- Warning-as-error syntax/type checks over the complete application tree for:
  - inhibited bench profile; and
  - fully acknowledged vehicle profile.
- Compile gates proving:
  - bench profile compiles;
  - vehicle without BPSD acknowledgement is rejected;
  - vehicle with BPSD but without CM200 acknowledgement is rejected; and
  - vehicle with both acknowledgements compiles.
- Project-structure, release-hygiene, ZIP-integrity, and shell-syntax checks.

Primary commands:

```sh
make -C host_tests CC=gcc clean
make -C host_tests CC=gcc ci
make -C host_tests CC=gcc asan
make -C host_tests CC=gcc ubsan
make -C host_tests CC=gcc stress
bash ci/scripts/check_core_host_syntax.sh
bash ci/scripts/check_core_gcc_analyzer.sh
bash ci/scripts/check_project_structure.sh
bash ci/scripts/check_repo_hygiene.sh
```

## Safety behavior covered

AMS:

- compact status/electrical/thermal/health decoding and endianness;
- independent 500 ms freshness;
- protocol and rolling-sequence guards;
- all relevant status/fault/thermal-block bits;
- cell, pack voltage/current, thermal, average, fan, location, and count plausibility;
- malformed required-frame immediate invalidation;
- legacy frame bounds/tails for all 75 cell slots and 85 temperature slots.

CM200:

- little-endian decoding of 11 broadcasts;
- required A5/A7/AA/AB/AC/B1 freshness;
- feedback-health versus VSM torque-ready split;
- CAN torque mode, lockout, direction, VSM state, POST/RUN fault gates;
- expected-counter acquisition, progression, one-command lag, wrap, mismatch debounce, and post-sync fault;
- current/previous command echo and mismatch fault;
- power-on timer progression, repeats, wrap, backwards/reset detection;
- startup grace, runtime loss, immediate fault, and reset-required latches;
- capability clamp, signed packet encoding, direction-preserving disable, unlock sequence, rolling counter, and slew limit;
- random broadcasts cannot create authority without correlated ECU commands.

System behavior:

- RTD initial release, deliberate press, early/stuck press rejection, momentary-button release, 2 s sound, fault exit, new-action rearm, and tick wrap;
- complete torque-gate fault matrix including CM200;
- BPSD fail-low semantics and 250 ms healthy recovery;
- task-heartbeat timeout/wrap behavior;
- bench/vehicle compile locks.

The full-source checks also cover the HAL/FreeRTOS integration of the one-slot transmit mailbox, final pre-transmit revalidation, bounded CAN FIFO drain, hardware filter configuration, CLI priority/stack diagnostics, cooling heartbeat, and stopped-flow freshness. Those integration paths still require target and hardware tests because they are not executed by the pure host harness.

## Not available in this environment

`arm-none-eabi-gcc`, STM32CubeIDE, Clang, and physical hardware were unavailable. Therefore this release does not include or claim validation of:

- STM32F767 target ELF, map, HEX, or binary;
- target ABI/linker/startup correctness beyond source checks;
- measured CPU load, interrupt latency, task jitter, stack margin, or watchdog time;
- bxCAN filter register behavior, bus electrical timing, termination, error recovery, or actual message rates;
- CM200 EEPROM settings, counter/echo semantics, VSM/precharge sequence, torque capability, or physical disable;
- output polarity/glitches, BPSD conditioning, BMS/IMD/TSAL/motor discrete levels;
- APPS/BSE/cooling calibration, brake performance, or vehicle dynamics.

No vehicle-release image should be produced from this source until a clean target build/map review and every applicable stage in `Core/docs/ECU_HARDWARE_BRINGUP.md` is complete.

## Required vehicle selection

The default remains:

```text
ECU_BUILD_PROFILE=0
```

Only after signed hardware evidence exists may a vehicle build define:

```text
ECU_BUILD_PROFILE=1
ECU_BSPD_INTERFACE_3V3_VALIDATED=1
ECU_CM200_CAN_CONTRACT_VALIDATED=1
```

The two acknowledgement symbols are interlocks only. They do not perform or certify the required tests.
