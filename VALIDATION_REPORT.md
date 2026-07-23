# DER26 ECU Firmware v2.5.1 Validation Report

Date: 2026-07-22

## Scope

This report covers the second clean compatibility pass between the ECU and DER26 AMS v0.3.4 before implementation of the torque-to-DC-current model. It validates compact AMS health frames, power protocol v2 consumption, legacy diagnostic layout, direction-aware final authority, hardware-commit timing, CAN acceptance filters, and build interlocks. It does not claim a numeric DCL/CCL-to-torque clamp, MPC, target runtime timing, inverter bench validation, or vehicle validation.

## Automated validation completed

- `host_tests/make ci`: focused unit tests, host regression, portable AMS power-consumer conformance, ECU power integration, AMS-v0.3.4 producer golden vectors, system SIL/fault injection, build-profile gates, and GCC `-fanalyzer`.
- 45 named unit/regression/SIL tests plus dedicated protocol-v2 conformance, ECU integration, and producer-golden-vector programs.
- Extended deterministic stress with 50,000 AMS sequence iterations and 50,000 CM200 random-frame iterations.
- AddressSanitizer plus UndefinedBehaviorSanitizer on the portable consumer, unit, regression, SIL, AMS power-integration, and producer-golden-vector suites.
- Standalone UndefinedBehaviorSanitizer on the same coverage.
- Clang static analyzer on portable-consumer, unit, regression, SIL, integration, and producer-golden-vector source sets.
- GCC full-application analyzer on all 35 bench-profile application files.
- Warning-as-error full-application syntax/type check for the bench profile.
- Explicit negative build check proving the vehicle profile remains locked even when every external evidence flag is supplied.
- Strict warning pass on the host-testable safety/parser core and changed AMS/CAN application path using conversion, sign-conversion, shadow, double-promotion, cast, format, undef, and float-equality warnings.
- Direct source-to-source compatibility test: the actual AMS v0.3.4 `ams_power_can.c` encoder was compiled separately, linked to the ECU consumer, and decoded successfully.
- GitHub workflow YAML parse and shell-script syntax checks.
- Repository structure, hygiene, and `git diff --check` gates.

## Compatibility and fault coverage

- Legacy AMS frame bounds and tails for all 75 cell slots, 120 temperature slots, and 10 fan slots; current map is cells `3-27`, temperatures `28-67`, fans `68-71`.
- Compile-time packet-layout assertions and rejection of unmapped headers inside the nominal packet range.
- Compact AMS `0x680-0x683` decoding, protocol/sequence/freshness/fault gates, electrical/thermal plausibility, and immediate malformed-frame invalidation.
- Electrical cross-consistency requiring the 0.1 V pack summary to lie within the 75-series min/max-cell bounds, allowing 200 mV for quantization/rounding.
- Power protocol v2 required bundle `0x684-0x687`: ID-bound CRC-8/SAE-J1850, version/counter checks, all frame orders, duplicate/partial/skew rejection, two-good-bundle qualification, 250 ms freshness, semantic limits, and independent discharge versus charge/regen authority.
- Advisory `0x689`/`0x68A` synchronization and containment: malformed advisory data invalidates only its own metadata and cannot revoke scalar authority.
- Exactly three wire horizons (`0.1/10/30 s`), with no fabricated one-second array element.
- Receive-path scalar-cache revocation immediately after malformed/CRC-invalid required power frames.
- Final CM200 transmit authority check performed after mailbox wait and at bxCAN hardware enqueue while CAN RX is masked.
- CAN freshness timestamps captured while RX is masked to prevent one-tick false-stale decisions.
- CM200 broadcast parsing, freshness, rolling counter, torque echo, timer progression, capability, and fault supervision.
- CAN filter list explicitly fills all 28 bxCAN 16-bit list slots; no implicit ID `0x000` entries remain. The stale permissive `MX_CAN1_Init()` filter was removed so `canbus_device_init()` is the single acceptance-filter owner before CAN start.
- CAN notification activation has one checked owner after filters are installed and CAN is started; activation failure latches startup fault, and structure CI prevents generated duplicate ownership.
- The ISR-owned CAN hardware-fault flag is consumed directly by both the APPS pre-gate and final command-commit gate, avoiding a lower-rate aggregation/clear race.

## Vehicle build status

Vehicle output remains deliberately compile-locked. `ECU_AMS_POWER_CLAMP_IMPLEMENTED` is source-owned and set to `0`; defining it externally is rejected. `ECU_AMS_POWER_CLAMP_VALIDATED` is a separate release-evidence acknowledgement. Both will be required after the conservative low-speed/stall-safe torque-to-DC-current model and numeric DCL/CCL clamp are implemented.

## Not completed in this environment

- ARM target ELF/link/map generation: `arm-none-eabi-gcc` is unavailable.
- Hardware CAN, CM200DX, motor, accumulator, WCET, stack-watermark, dyno, or vehicle tests.
- Numeric battery-current-to-torque enforcement and MPC.
