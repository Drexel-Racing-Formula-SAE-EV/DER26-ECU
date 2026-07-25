# DER26 ECU Firmware v2.5.2 Release Notes

Date: 2026-07-25

## Scope

This is a focused clean compatibility pass over v2.5.1. The AMS power protocol-v2 wire format is unchanged. The numeric torque-to-pack-current clamp is still not implemented, so the vehicle profile remains source-locked.

## Changes

- Made optional AMS `0x689` strategy and `0x68A` binding metadata association independent of whether the advisory arrives before or after the final required core frame.
- Made advisory/core skew comparison correct across the 32-bit millisecond tick wrap.
- Added regression tests for advisory-before-core and wrap-boundary behavior.
- Renamed the locked producer golden-vector suite to AMS v0.3.6 compatibility.
- Added `make power-source-compat AMS_ROOT=/path/to/AMS`, which compiles the live AMS producer with the ECU consumer and verifies exact payload and decoded-state compatibility.
- Updated repository structure gates and current documentation.

## Deliberate blockers

- `ECU_AMS_POWER_CLAMP_IMPLEMENTED` remains source-owned and zero.
- Vehicle output remains inhibited.
- No CM200 bench, target timing, pack-current model, dyno, or vehicle claims are made.
