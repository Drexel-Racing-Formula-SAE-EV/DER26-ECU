# DER26 ECU Firmware v2.6.2 Validation Report

**Date:** 2026-07-25

## Passed host validation

- GCC CI: torque clamp, residual monitor, elapsed timer, Python/C differential model, unit tests, integration tests, protocol golden vectors, system SIL, profile gates, static-allocation gate, GCC analyzer.
- Clang CI and Clang static analyzer.
- Clang AddressSanitizer + UndefinedBehaviorSanitizer suites.
- 50,000-cycle deterministic system-SIL stress test.
- Live source compatibility against DER26 AMS v0.3.5.

## Target validation not performed

No ARM target compiler was available in the execution environment. This report does not claim:

- final ELF/map success;
- flash/RAM placement;
- task stack high-water marks;
- interrupt latency;
- DWT WCET on STM32F767;
- physical CAN timing;
- hardware pin/electrical validation.

## Release disposition

Suitable for controlled low-voltage bench integration after a normal STM32 target build. Not suitable for torque-enabled or HV vehicle release until all evidence gates are closed.
