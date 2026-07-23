# DER26 ECU v2.5.1 — AMS Compatibility Second Clean Pass

## AMS and power-protocol corrections

- Updated legacy AMS telemetry from 17 to 24 temperatures per segment: 120 total values, headers `28-67`, fan headers `68-71`, and packet count 72.
- Added compile-time layout assertions and rejection of any nominally in-range legacy packet header that maps to no real destination.
- Updated compact `0x683` health validation for sensor indices `0-23` and usable-temperature counts through 120.
- Prevented malformed optional `0x689`/`0x68A` frames from clearing valid `0x684-0x687` scalar authority.
- Refreshes task-facing authority immediately after every required power-frame ingest so malformed/CRC-invalid required data fails closed in the receive path.
- Made final authority direction-aware: positive torque requires discharge authority and negative torque requires charge/regen authority.
- Allows the common AMS health gate to remain healthy when only one direction is authorized; the final command gate still rejects the unauthorized direction.
- Rejects nonfinite current or power limits.
- Added a 75-series pack-voltage consistency check against transmitted min/max cell bounds, with a 200 mV encoding tolerance.
- Added an AMS-v0.3.4 producer-generated golden-vector test covering exact bytes, CRC, scales, three-horizon selection, SoH, resource state, and binding metadata.

## CAN and command-path corrections

- Explicitly filled all 28 bxCAN filter-list entries; the previous partial initializer unintentionally accepted standard ID `0x000` in three slots.
- Removed the separate permissive filter from `MX_CAN1_Init()` so filter setup has one owner before CAN start.
- Removed duplicate pre-start CAN notification activation. `app_create()` now owns one checked post-start activation, and activation failure latches startup fault.
- Added structure checks that reject regenerated permissive filters or duplicate notification activation.
- Fixed a one-tick false-stale race by capturing CAN freshness time while RX is masked.
- Moved the final AMS/CM200 authority re-read to the hardware-commit point: after any mailbox wait and before bxCAN enqueue, under one CAN-RX-masked critical section.
- The APPS gate and final commit gate now consume the ISR-owned CAN hardware-fault flag immediately instead of waiting for the lower-rate aggregate fault.
- Removed a redundant nested FreeRTOS critical section in the error task.

## Build, diagnostics, and CI

- Added source-owned `ECU_AMS_POWER_CLAMP_IMPLEMENTED=0`, which cannot be overridden through compiler flags.
- Vehicle builds require both implementation and validation latches and remain intentionally blocked until the low-speed/stall-safe numeric clamp exists.
- Corrected host/profile scripts and ARM-build evidence-flag handling.
- Hosted CI explicitly runs protocol conformance, ECU integration, producer golden vectors, standalone UBSan, full bench syntax/analyzer checks, and sanitized portable-consumer coverage.
- Manual GitHub checkout uses the workflow token and exact `GITHUB_SHA`.
- RTC CLI writes validate date/time ranges and leap years, reject malformed values before narrowing, and report HAL write failures.

## Validation

- Full host CI, ASan/UBSan, standalone UBSan, 50k-cycle stress, GCC analyzer, Clang analyzer, full bench syntax check, strict warning passes, workflow/script parsing, project structure, repository hygiene, and diff checks passed.
- A direct source-to-source cross-test linked the real AMS v0.3.4 encoder to the ECU consumer and passed.
- No torque-to-DC-current model or MPC was added.
- No local ARM target or hardware validation is claimed.
