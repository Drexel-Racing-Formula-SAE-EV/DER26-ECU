# DER26 ECU Firmware v2.6.1 Validation Report

Date: 2026-07-25

## Disposition

The v2.6.1 source completes the planned ECU torque-to-pack-current software architecture and closes the contract-breaking gaps found in v2.6.0. The implementation latch is enabled, but the vehicle validation latch and numerical calibration remain intentionally closed.

This report covers portable/host validation only. It is not a target or vehicle release record.

## Source behavior reviewed

- direct whole-cell path enumeration;
- schema-enforced 21-point/20-cell maximum;
- steady uncertainty and operating-region union;
- transition profile/direction/full-span lookup;
- transition monotonic containment validation;
- bounded transition refinement;
- raw/normalized torque separation;
- zero hysteresis and persistent sign/reversal state;
- settled tracking, microstep margin, cumulative drift, settling, and re-anchor;
- battery-authority states;
- boot-only CRC qualification;
- APPS producer versus CAN hardware-commit ownership;
- comparison-only final DCL/CCL/capability/generation verification;
- state update after hardware-mailbox acceptance;
- canonical pack-current residual monitoring;
- source-independent AMS current behavior;
- runtime `power` CLI diagnostics;
- fixed provisional healthy-R2D auxiliary interval.

## Commands and results

### Full portable suite and Clang static analysis

```bash
make -C host_tests CC=clang CLANG=clang clang-ci
```

Passed:

- torque-clamp contract suite;
- residual-monitor suite;
- 12 independent Python/C current-model vectors;
- focused ECU unit tests;
- host regression tests;
- AMS power-consumer conformance;
- AMS/ECU power integration;
- AMS v0.3.6 golden vectors;
- system SIL/fault injection;
- bench/vehicle profile gates;
- Clang static analyzer with no reported findings.

The torque-clamp suite includes 20,000 deterministic randomized contract iterations and checks torque quantization, CM200 capability limits, current authorization, transition requirements, and execution-count bounds.

### Sanitizers

```bash
make -C host_tests CC=clang asan
make -C host_tests CC=clang ubsan
```

All included parser, safety, SIL, protocol, clamp, and residual-monitor binaries passed AddressSanitizer and UndefinedBehaviorSanitizer. Leak detection was disabled for the FreeRTOS-style host harnesses as configured by the repository target.

### Extended deterministic stress

```bash
make -C host_tests CC=clang stress
```

Passed the 50,000-cycle deterministic system SIL/fault-injection run.

### Full application host syntax

```bash
CC=clang bash ci/scripts/check_core_host_syntax.sh
```

All 39 `Core/Src` C files parsed with warnings treated as errors for:

- the inhibited bench profile;
- the fully acknowledged vehicle source profile.

The default vehicle profile remained blocked by missing external evidence acknowledgements.

### Live AMS compatibility

```bash
make -C host_tests CC=clang power-source-compat \
  AMS_ROOT=/path/to/DER26-AMS-v0.3.6/AMS
```

Passed live compilation of the AMS v0.3.6 `ams_power_can.c` producer with the ECU consumer and exact protocol-v2 payload/decoding checks.

### Residual timestamp limitation

Protocol v2 does not carry the physical AMS current-sample timestamp. The ECU therefore uses the coherent electrical-frame receive timestamp. AMS acquisition, filtering, publication, CAN, and receive jitter must be bounded by the certification campaign before vehicle validation. Automatic mid-run source failover remains disabled, so the first-release source epoch is fixed at boot.

## Corrected v2.6.0 defects

1. Whole-cell search no longer infers traversed-cell coverage from boundary point lookup.
2. The actual bxCAN commit rechecks cached numerical current intervals against the newest DCL/CCL.
3. Clamp state no longer advances before hardware-mailbox acceptance.
4. Runtime search no longer recalculates the complete calibration CRC.
5. Active transitions now settle and re-anchor deterministically.
6. Tracking includes rate, band, anchor, drift, and physical-state conditions.
7. Input age/uncertainty and region union are implemented.
8. Transition profile/direction axes and monotonic containment are implemented.
9. Optional increase transition refinement is implemented.
10. Residual monitoring is connected to runtime command/current data.
11. Battery-authority states are implemented.
12. Python/C differential and broader contract/property tests are present.

## Release locks and checked-in calibration

```text
ECU_AMS_POWER_CLAMP_IMPLEMENTED = 1
ECU_AMS_POWER_CLAMP_VALIDATED   = 0
```

The checked-in calibration deliberately has:

```text
evidence_valid        = false
torque_axis_points    = 0
steady_cell_count     = 0
transition_cell_count = 0
crc32                 = 0
```

Therefore the production runtime cannot qualify the artifact or grant nonzero vehicle torque from it.

## Explicitly not performed

Per the task constraint and available environment:

- no GCC host validation was attempted;
- no `arm-none-eabi-gcc` target build was attempted;
- no ELF/map/stack target review was performed;
- no flash or hardware test was performed;
- no HIL, dyno, or vehicle campaign was performed;
- no APM-authoritative or automatic failover claim was made.

## Remaining evidence before vehicle release

- final canonical DHAB/APM branch-return drawing;
- certified healthy R2D auxiliary bound;
- steady whole-cell calibration including low-speed/stall/four-quadrant cases;
- transition/composed-sequence and microstep holdout evidence;
- target WCET, stack, ISR-interference, deadline, and map records;
- CM200 command timeout and EEPROM contract;
- HIL, dyno, restricted vehicle, and APM-primary validation;
- formal margin-ledger and release review.
