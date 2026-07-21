# DER26 ECU Firmware v2.3.0 Release Notes

## Major corrections

- Corrected STM32 HAL handle ownership from copied structs to pointers to Cube-generated global handles. This fixes CAN callback identity and prevents peripheral state divergence.
- Corrected PE13 from misleading `BSPD_Fail` semantics to active-high `BSPD_OK` with fail-low pull and recovery debounce.
- Added compile-time bench/vehicle profiles and locked vehicle output release behind BPSD interface acknowledgement.
- Added independent freshness and sanity gates for AMS status, electrical, and thermal compact frames.
- Added AMS thermal overtemperature/severe/invalid torque blocking.
- Added CM200 rolling counter and constant-direction disable packets.
- Added CAN controller error/FIFO overrun monitoring.
- Made the error task the sole owner of shutdown and inverter hardware outputs.
- Added task heartbeat supervision and a vehicle-profile independent watchdog.
- Added deterministic fail-safe handling for exceptions, RTOS stack overflow/allocation failure, assertions, and HAL fatal errors.
- Added checked ADC acquisition and immediate propagation of conversion/channel errors.
- Increased sensor ADC acquisition from 3 to 56 cycles to suit the conditioned/divided analog inputs and shared-channel settling.
- Moved APPS plausibility to current raw normalized samples while retaining filtered torque demand.
- Fixed APPS moving-average startup and percent-to-16-bit conversion.
- Fixed MPU6050 signed sample decoding and power-management register naming/use.
- Removed false coolant temperature units; telemetry remains invalid until calibration and the pump defaults to 100%.
- Expanded CLI with `ver`, `status`, `ams`, `can`, and `bspd` diagnostics.
- Added build-profile CI gates, expanded SIL fault injection, and hardware bring-up documentation.

## Default behavior change

Debug, Release, and headless builds default to `ECU_BUILD_PROFILE=0`. They cannot enable the inverter or request nonzero torque. This is intentional.

## Remaining hardware work

The target ARM binary, BPSD interface, all discrete polarities, ADC calibration, real AMS traffic, CM200 behavior, watchdog timing, and vehicle shutdown behavior must be validated on hardware before vehicle-profile use.
