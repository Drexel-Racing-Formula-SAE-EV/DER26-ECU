# DER26 ECU Firmware v2.6.2 Release Notes

**Date:** 2026-07-25  
**Disposition:** Bench integration release; vehicle torque remains evidence-locked.

## Purpose

v2.6.2 implements the integration hardening identified during the full v2.6.1 review without redesigning the completed deterministic torque clamp.

## Implemented changes

### Static RTOS memory

- Converted all ten application tasks to `xTaskCreateStatic()`.
- Converted the latest-value CAN transmit queue to `xQueueCreateStatic()`.
- Added static control-block storage for all six CMSIS mutexes.
- Disabled FreeRTOS dynamic allocation for the application configuration.
- Added a CI gate that rejects dynamic application task/queue creation and verifies exactly ten statically created tasks.

### Distinct-sample residual monitoring

- Residual violation, recovery, and source-settling counters now advance only on a new current-measurement sequence.
- Stale/invalid current remains a time-domain immediate fault.
- Added source-epoch handling and bounded source-settling behavior.
- Increased the provisional current freshness window from 100 ms to 200 ms so it is not equal to the nominal 10 Hz publication period.
- Added optional `0x68B` source metadata consumption: source, quality, boundary, epoch, sample sequence, and physical sample age.
- Source identity is advisory only and cannot grant torque authority or select a different model.

### Driver-input timing

- Replaced APPS and dual-brake sample counters with wrap-safe elapsed-time timers.
- Raised the brake-sensor task to 100 Hz.
- Uses a 90 ms observed mismatch threshold to guarantee response no later than 100 ms including one scheduler-period phase uncertainty.
- Added exact below/at/above-threshold and tick-wrap host tests.

### Target timing instrumentation

- Added Cortex-M7 DWT cycle-counter initialization and conversion helpers.
- Records last and maximum clamp execution cycles.
- Adds a 3 ms soft budget and 8 ms hard threshold for the 10 ms task period.
- Every hard overrun forces zero; two consecutive hard overruns latch a torque-clamp overrun fault that reaches the error supervisor.
- CLI power diagnostics report cycle counts, soft/hard overruns, and the latched overrun state.

### Release gates

Vehicle compilation now requires independent evidence acknowledgements for:

- pin mapping;
- APPS and brake calibration;
- discrete inputs;
- AMS protocol;
- current model and residual monitor;
- cooling;
- RTOS memory;
- WCET;
- CAN load;
- watchdog;
- safe outputs.

The existing BSPD, CM200, and AMS power-clamp gates remain active.

## Preserved architecture

The following v2.6.1 behavior is unchanged:

- separate steady and transition current models;
- persistent movement/reversal/transition state;
- physical-zero reversal requirement;
- zero torque always legal;
- bounded search and refinement;
- late comparison-only verification;
- state update only after bxCAN mailbox acceptance;
- one-entry latest-value command queue;
- invalid current-model calibration blocks nonzero torque.

## Deliberate locks

The checked-in image is not vehicle-authoritative:

- current-model calibration remains invalid;
- current measurement uncertainty in the ECU residual monitor is provisional;
- cooling conversion/fault policy remains unvalidated;
- all new release evidence gates default to zero;
- no ARM target build, map, stack high-water, or DWT WCET evidence is claimed.
