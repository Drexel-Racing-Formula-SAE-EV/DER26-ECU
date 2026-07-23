# DER26 ECU / AMS v0.3.4 Second Clean Compatibility Pass

**Date:** 2026-07-22  
**ECU result:** v2.5.1  
**Scope:** AMS/SoP CAN compatibility, fail-closed command gating, and validation infrastructure only. No torque-to-DC-current model or MPC was added.

## Executive result

The ECU now matches the AMS v0.3.4 compact-health and power-protocol interfaces closely enough to begin the conservative torque-to-DC-current work without carrying known stale layout or CAN-authority defects forward. The portable protocol-v2 consumer remains byte-aligned with the AMS reference implementation, apart from an ECU-only explicit-ID invalidation wrapper used when the hardware receive path detects malformed known frames.

This second pass found and corrected issues beyond the first v2.5.0 integration: stale 120-temperature mapping, optional-frame containment, immediate required-frame revocation, direction-specific authority, an externally bypassable release acknowledgement, implicit CAN ID `0x000` filters, two CAN timestamp/commit races, a missing pack-voltage-versus-cell-bound consistency check, duplicate/unchecked CAN notification ownership, and a transient CAN-hardware-fault visibility gap at the command gate. CI was also corrected so the protocol consumer, producer-generated golden vectors, integration suites, sanitizers, stress tests, and analyzers are actually exercised in hosted automation.

## Source-of-truth reconciliation

Compared directly against AMS v0.3.4:

- Compact health frames: `0x680-0x683`.
- Required protocol-v2 authority: `0x684-0x687`.
- Mission request: `0x688`.
- Optional strategy/resource and binding metadata: `0x689`, `0x68A`.
- Wire horizons: exactly `0.1`, `10`, and `30 s`; no fabricated one-second slot.
- Legacy diagnostic stream: five segments, 15 cells/segment, 24 temperatures/segment, ten fans, packet headers `0-71`.
- Required power frames remain atomic, CRC/counter/freshness protected, and qualified by two consecutive good bundles.
- `0x689` and `0x68A` remain advisory and cannot revoke valid scalar authority.

## Defects corrected

1. **Stale legacy temperature layout.** ECU arrays, packet bounds, compact-health counts, docs, and tests assumed 17 temperatures/segment and 85 total. They now match 24/segment and 120 total. Compile-time assertions tie the packet count to the physical layout.
2. **Silent in-range packet discard.** A legacy header below `AMS_PACKET_COUNT` but not mapped to any destination was previously accepted. Such a layout/count mismatch is now rejected and counted malformed.
3. **Advisory malformed-frame containment.** Wrong-DLC, remote, CRC, version, or semantic failures on `0x689`/`0x68A` invalidate only the affected advisory cache and do not clear `0x684-0x687` scalar authority.
4. **Immediate required-frame revocation.** The task-facing scalar cache refreshes in the receive path after every required-frame ingest. A malformed/CRC-invalid required frame becomes fail-zero immediately rather than waiting for the 100 Hz error task.
5. **Direction-aware final authority.** Positive commands require discharge authority; negative commands require charge/regen authority. The common health gate accepts either usable direction, while the command-specific gate selects exactly one. Nonfinite limits are rejected.
6. **Unbypassable missing-clamp lock.** `ECU_AMS_POWER_CLAMP_IMPLEMENTED` is source-owned and remains `0`; compiler flags cannot override it. External validation acknowledgement alone cannot make a vehicle profile compile before numeric clamp code exists.
7. **CAN filter ownership and zero-ID leakage.** The seven-bank, 28-slot bxCAN list previously left three entries implicitly zero, admitting standard ID `0x000`. Every slot is now explicitly populated and guarded by `_Static_assert`. A separate permissive ID-mask filter in `MX_CAN1_Init()` was removed so the driver is the single filter owner before `HAL_CAN_Start()`; this eliminates a regeneration/configuration hazard even though the old catch-all was overwritten before CAN start in the current boot order.
8. **Receive-time freshness race.** CAN freshness timestamps sampled before masking RX could be one tick older than a frame accepted immediately before the critical section, causing false stale detection through unsigned age arithmetic. AMS/CM200 stale updates now use a timestamp captured while RX is masked.
9. **Final-command commit race.** Authority was previously re-read before a possible mailbox wait. A newer AMS zero accepted during that wait could be bypassed by the older local torque candidate. The task now waits for a free mailbox first, then masks CAN RX, captures a coherent time, re-reads AMS/CM200 authority, converts to disable when necessary, and enqueues to bxCAN before unmasking RX.
10. **Electrical-summary cross-consistency gap.** A numerically valid min/max cell pair could be accepted with an impossible pack voltage, including zero. The ECU now requires the reported pack voltage to lie between `75 * min_cell` and `75 * max_cell`, with a 200 mV allowance for source/encoding rounding. This is a data-integrity check, not an operational voltage limit.
11. **CI coverage gaps.** Hosted CI now runs protocol-v2 conformance and ECU integration explicitly, includes standalone UBSan, sanitizes the portable consumer itself, validates the full bench application, and uses the exact GitHub commit/token during manual checkout. Project-structure checks require the protocol consumer, tests, and CAN documentation.
12. **Critical-section cleanup.** A redundant nested critical section in the error task was removed.
13. **CAN notification activation ownership.** Generated `main.c` no longer activates CAN notifications before the driver-owned filters/start sequence. `app_create()` is now the single checked post-start owner; failure or a non-started CAN device latches `startup_fault`. The structure gate rejects a regenerated catch-all filter or duplicate notification activation.
14. **Immediate CAN hardware-fault visibility.** The APPS pre-gate and final hardware-commit gate now OR the ISR-owned `canbus_hw_fault` directly into the torque fault input. This prevents a transient error from being missed before the 100 Hz error task aggregates it, especially because the CAN task may clear a recovered transient after a successful send.
15. **Producer-generated protocol golden vectors.** A permanent test now decodes two consecutive complete frame sets generated by the actual AMS v0.3.4 `ams_power_can.c` producer. It locks CRC, byte order, scaling, the three transmitted horizons, SoH, resource state, and per-horizon binding metadata. A separate one-time source-to-source build also linked the real AMS encoder directly to the ECU decoder and passed.
16. **CLI date/time validation.** RTC writes now reject out-of-range year/month/day/time values, including invalid month lengths and non-leap-year February 29, and report HAL write failures. A parameter-shadow warning in the CLI task entry point was also removed.

## Validation completed

- `host_tests/make ci` passed.
- 45 named unit/regression/SIL tests passed, plus dedicated portable-consumer conformance, ECU/AMS integration, and AMS-v0.3.4 producer-golden-vector executables.
- AddressSanitizer plus UndefinedBehaviorSanitizer passed for the portable consumer, focused unit suite, regression suite, system SIL, power-integration suite, and AMS-v0.3.4 producer-golden-vector suite.
- Standalone UndefinedBehaviorSanitizer passed for the same coverage, including the producer-golden-vector suite.
- Extended deterministic stress passed with 50,000 AMS sequence iterations and 50,000 CM200 random-frame iterations.
- GCC `-fanalyzer` passed on host-testable safety/protocol sources and on the complete 35-file bench application.
- Clang static analyzer passed on protocol, unit, regression, SIL, power-integration, and producer-golden-vector source sets.
- Strict conversion/sign/shadow/double-promotion/cast/format/float-equality warning passes succeeded for the host-testable safety core and the changed AMS/CAN application path.
- Full warning-as-error bench application syntax/type check passed.
- The actual AMS v0.3.4 encoder was compiled in a separate translation unit and linked directly to the ECU consumer; the end-to-end producer-to-consumer cross-test passed.
- Vehicle-profile negative tests proved that all external acknowledgements still cannot bypass the source-owned clamp implementation lock.
- GitHub workflow YAML and all shell scripts passed syntax parsing.
- Repository structure, hygiene, and diff checks passed before packaging.

## Remaining deliberate blocker

The ECU still does not numerically translate AMS DCL/CCL and power limits into a conservative motor-torque ceiling, especially at zero and low speed. Therefore the vehicle profile remains intentionally unbuildable and no nonzero vehicle release is claimed. The next development item is the conservative torque-to-DC-current upper-bound model, independent oracle/property tests, and deterministic 100 Hz numeric clamp.

## Not validated here

- STM32 ARM ELF/link/map, because `arm-none-eabi-gcc` is unavailable in this environment.
- Hardware CAN timing, CM200DX behavior, motor/accumulator operation, WCET, stack watermark, or vehicle behavior.
- Numeric current-to-torque enforcement or MPC.
