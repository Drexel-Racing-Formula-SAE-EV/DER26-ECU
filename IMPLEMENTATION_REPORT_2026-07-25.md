# DER26 AMS/ECU Implementation Report

**Date:** 2026-07-25  
**Implemented baselines:** AMS v0.3.4 → v0.3.5; ECU v2.6.1 → v2.6.2

## Implemented now

### AMS

- Added the canonical source-independent pack-current sample in fixed milliamp units.
- Added DHAB and APM provider interfaces.
- Wrapped the existing DHAB dual-range ADC path as a canonical provider with explicit uncertainty.
- Added a deliberately fail-closed APM provider stub; it cannot become authoritative until redundant ADBMS2950 current channels and calibration exist.
- Added CRC-protected boot source configuration with invalid zero default.
- Added authoritative-primary and comparison-only-shadow operation.
- Prohibited automatic failover.
- Required physical zero for deliberate source changes and incremented source epoch.
- Added time-aligned shadow interval comparison.
- Routed canonical current into the existing current window, estimators, SoP, fuse logic, safety, and CAN.
- Preserved the provider physical sample timestamp through downstream consumers.
- Added advisory CAN frame `0x68B` containing source/quality/boundary/epoch/sample-sequence/sample-age metadata.
- Added CLI diagnostics and source-config generator tooling.
- Added vehicle release gates and host tests for invalid defaults, disabled providers, source switching, shadow authority, and source epochs.

### ECU

- Converted all ten tasks, the CAN latest-value queue, and six mutexes to static allocation.
- Disabled FreeRTOS dynamic allocation in the application configuration.
- Corrected residual monitoring so persistence advances only on distinct physical current samples.
- Added source-epoch reset/settling behavior and time-based stale faulting.
- Added `0x68B` parsing and physical sample-time reconstruction while keeping source identity advisory.
- Increased provisional measurement freshness to 200 ms so it is not equal to the 10 Hz nominal AMS period.
- Replaced APPS and brake sample counters with wrap-safe elapsed-time timers.
- Increased brake sensing to 100 Hz and added exact 100 ms boundary tests.
- Added Cortex-M7 DWT cycle instrumentation, soft/hard execution budgets, zero-on-overrun behavior, persistent-overrun fault escalation, and CLI reporting.
- Expanded vehicle release evidence gates across the full safety-relevant ECU.
- Preserved the existing deterministic steady/transition clamp and late hardware-mailbox commit architecture.

## Deliberate fail-closed states

The source packages are not vehicle-authoritative:

- AMS source configuration is the all-zero invalid sentinel.
- APM current authority is unavailable.
- ECU pack-current calibration is invalid.
- ECU residual measurement uncertainty is provisional.
- Cooling remains unvalidated.
- Hardware pin mapping, canonical current boundary, target stack/WCET, CAN load, watchdog, CM200, HIL/dyno, and vehicle evidence are not fabricated.

## Validation completed

### ECU

- GCC CI passed.
- Clang CI/static analysis passed.
- Clang ASAN/UBSAN passed.
- 50,000-cycle deterministic SIL stress passed.
- Live AMS producer/ECU consumer compatibility passed.
- Static-allocation gate passed.
- Exact elapsed-time boundary tests passed.

### AMS

- Unit tests passed.
- Canonical current-manager tests passed.
- Comprehensive host SIL/fault-injection tests passed.
- SoP/SoH core tests passed.
- 20,000 discharge + 20,000 charge metamorphic states passed.
- Independent fuse oracle/replay passed.
- Source profile/release gates passed.
- Whole-source GCC analyzer passed in bench and vehicle profiles.
- GCC ASAN/UBSAN passed.
- Extended seeded stress passed.
- Generated source-config artifact was compiled and CRC-validated against the C implementation.

A Clang-only AMS ASAN aggregate target remains blocked by existing test-oracle `-Wdouble-promotion` warnings in the long-double fuse reference fixture. The same production and host paths passed GCC ASAN/UBSAN; this is a host-test compile-warning issue, not an observed sanitizer runtime fault.

## Validation not possible here

No ARM GCC target toolchain was installed. The final STM32 ELF/map, linker placement, stack high-water marks, DWT WCET, ISR latency, physical CAN timing, and hardware measurements still require the team target environment.

## Required next physical work

1. Confirm the exact APM shunt and DHAB aperture boundary and every branch return.
2. Generate and review a DHAB-primary/APM-shadow source configuration only after boundary/sign/calibration evidence.
3. Bring up ADBMS2950 redundant current channels and implement the certified APM provider.
4. Collect ECU steady and transition current calibration data.
5. Measure STM32 task stack margins, clamp WCET, ISR interference, and CAN utilization.
6. Validate APPS, brake, cooling, BSPD, discrete inputs, watchdog, safe outputs, and CM200 configuration.
7. Run restricted torque HIL/dyno and vehicle fault injection before closing any release gate.
