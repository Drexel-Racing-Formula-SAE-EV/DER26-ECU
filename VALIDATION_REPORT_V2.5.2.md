# DER26 ECU Firmware v2.5.2 Validation Report

Date: 2026-07-25

## Scope

This report covers a clean compatibility pass over ECU v2.5.1 against AMS v0.3.6 power protocol v2. The wire protocol is unchanged. The pass corrects optional advisory association when advisory frames arrive before the final required core frame or across the 32-bit tick wrap, and adds a permanent source-to-source AMS producer/ECU consumer compatibility target. It does not claim a numeric torque-to-pack-current clamp, target timing, CM200 bench validation, or vehicle validation.

## Defects corrected

1. **Advisory arrival-order dependency.** Same-counter `0x689`/`0x68A` metadata received shortly before the final required bundle frame was rejected because skew was calculated with a one-direction unsigned subtraction. Advisory association is now order-independent inside the 50 ms certified skew window.
2. **Advisory/core skew across tick wrap.** Association now uses shortest modular timestamp distance and remains valid across the 32-bit millisecond wrap.
3. **Compatibility checking depended on a manually reproduced one-off step.** `make power-source-compat AMS_ROOT=/path/to/AMS` now compiles the live AMS `ams_power_can.c` producer with the ECU consumer, compares exact payloads for two counters, and validates decoded authority, envelope, and resource data.
4. **Golden-vector naming was stale.** The locked producer suite now identifies AMS v0.3.6 while retaining the unchanged protocol-v2 bytes.

## Automated validation completed

The following targets passed from `host_tests`:

- `make unit`
- `make test`
- `make power-consumer`
- `make power-integration`
- `make power-golden`
- `make power-source-compat AMS_ROOT=<AMS v0.3.6 source>`
- `make system-sil`
- `make profile-gates`
- `make analyze`
- `make clang-analyze`
- `make asan`
- `make ubsan`
- `make stress`

Additional checks:

- `ci/scripts/check_project_structure.sh`: PASS.
- All repository shell scripts parsed with `bash -n`: PASS.
- Vehicle profile remains locked without the source-owned numeric AMS clamp implementation latch: PASS.
- Live AMS v0.3.6 producer and ECU consumer exact-payload compatibility: PASS.

## Compatibility and safety behavior

- Required `0x684`-`0x687` bundle remains atomic, CRC/version/counter protected, two-good-bundle qualified, and stale after 250 ms.
- Optional `0x689`/`0x68A` data remains non-authoritative and cannot revoke scalar DCL/CCL authority when malformed.
- Advisory data is usable only when its counter matches the active bundle, its age is valid, and shortest modular timestamp skew is no greater than 50 ms.
- The final CM200 hardware-commit authority reread remains direction-aware and fail-zero.
- The ECU behavior remains independent of whether AMS canonical current originates from DHAB or APM; source identity is not part of the torque-gate equations.

## Vehicle build status

Vehicle output remains intentionally compile-locked. `ECU_AMS_POWER_CLAMP_IMPLEMENTED` is source-owned and remains `0`. `ECU_AMS_POWER_CLAMP_VALIDATED` remains a separate evidence gate. This clean pass does not implement the conservative low-speed/stall-safe torque-to-pack-current model or numeric DCL/CCL torque clamp.

## Not completed in this environment

- ARM target ELF/link/map generation because `arm-none-eabi-gcc` is unavailable.
- Hardware CAN, CM200DX, motor, accumulator, WCET, stack-watermark, dyno, or vehicle tests.
- Numeric pack-current-to-torque enforcement or MPC integration.
