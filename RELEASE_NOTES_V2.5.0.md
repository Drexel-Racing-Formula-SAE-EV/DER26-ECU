# DER26 ECU v2.5.0 — AMS Power Protocol v2 Integration

## Scope

This release updates the ECU AMS interface to match DER26 AMS v0.3.4 power protocol v2 before MPC development.

## Added

- Atomic fail-zero consumer for AMS IDs `0x684`-`0x687`.
- Optional synchronized strategy/resource frame `0x689`.
- Optional synchronized per-horizon binding metadata frame `0x68A`.
- Typed APIs separating immediate scalar authority, three wire horizons (`0.1/10/30 s`), SoH, and AMS-owned resource state.
- CRC-8/SAE-J1850 bound to CAN ID and payload.
- Two-good-bundle startup/recovery qualification, modulo-16 counter progression, 50 ms bundle skew, and 250 ms freshness.
- Mission-request encoder for `0x688`.
- Final CM200 transmit-path re-read of the newest scalar AMS authority.
- Vehicle-profile evidence interlock requiring `ECU_AMS_POWER_CLAMP_VALIDATED=1`; v2.5.1 later adds a source-owned implementation latch that compiler flags cannot bypass.
- Power protocol conformance and ECU integration tests.

## Important limitation

The ECU now rejects stale, malformed, inhibited, fallback, or zero AMS power authority before CM200 transmission. It does not yet numerically convert DCL/CCL and power limits into a torque ceiling. Vehicle output remains build-locked until the conservative low-speed/stall-safe torque-current model and final numeric clamp are implemented and validated.

## Compatibility

- Legacy paged AMS telemetry `0x069` and estimator frame `0x421` remain diagnostic only.
- Compact health/status frames `0x680`-`0x683` remain required for BMS health gating.
- Dynamic power authority uses protocol version 2 and exactly three wire horizons. No 1-second wire slot is fabricated.
