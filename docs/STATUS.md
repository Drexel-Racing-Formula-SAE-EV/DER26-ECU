# Current Source Status

## Current revision

- Firmware version: **2.10.7**
- Source revision: `DER26-ECU-v2.10.7-SAFETY2-20260827`
- CAN contract: `DER26-CAN-V4`
- Logger schema: `LOGGER3`

The v2.10.7 delta on top of the latest complete v2.10.6 source closes the ECU-side CAN status/error interrupt gap identified in the August safety review and updates runtime/source provenance.

## Current software state

Implemented software includes:

- APPS/brake/discrete vehicle-state supervision;
- AMS power-authority consumer and coherent freshness handling;
- CM200 state/capability supervision;
- bounded torque-to-pack-current model and residual monitor;
- CAN TX mailbox ownership/commit tracking;
- dedicated CAN TX/RX0/SCE interrupt coverage;
- coolant sensing/pump control infrastructure;
- SD/FatFs service and decoded/raw-CAN logger;
- service CLI/dashboard interfaces;
- host unit/SIL/stress/sanitizer/static-analysis infrastructure;
- headless ARM-GCC CI build path.

## Deliberate qualification gaps

The repository should not be interpreted as proof that every vehicle-authority evidence gate is complete. Remaining target/system evidence includes, as applicable to the selected build/profile:

- target task stack and interrupt-stack margin;
- target WCET/scheduling jitter;
- physical CAN error/bus-off recovery validation;
- APPS/brake/discrete calibration evidence;
- CM200 configuration/timeout validation;
- pack-current model certification across the required operating region;
- current-residual calibration/acceptance evidence;
- coolant sensor/pump validation;
- watchdog and reset-fault behavior;
- safe-output electrical validation;
- HIL/dyno/vehicle qualification.

## Repository policy

Current design/qualification information belongs in maintained documents under `docs/`. One-off patch notes, obsolete release reports, IDE build outputs, and local debug metadata are intentionally not part of the active repository.
