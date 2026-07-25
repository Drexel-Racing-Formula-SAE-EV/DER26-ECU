# DER26 ECU Firmware v2.6.1 Release Notes

## Scope

v2.6.1 is the corrective completion pass for the torque-to-pack-current plan. It replaces the partial v2.6.0 scaffold with the complete bounded software contract while deliberately leaving vehicle calibration and external evidence locks closed.

## Corrected contract gaps

### Whole-cell path proof

- Increase search now enumerates direct torque-cell indices.
- It evaluates every complete cell touched by the raw path and stops at the first unproven/infeasible cell.
- Calibration dimensions are capped at 21 points / 20 cells and rejected above that limit.

### Actual hardware-mailbox commit

- APPS produces a torque contract but no longer mutates committed state.
- The CAN task waits for a free bxCAN mailbox, snapshots the newest AMS/CM200 authority and generations, and performs comparison-only cached-interval verification.
- DCL/CCL, direction authorization, CM200 capability, calibration, speed, Vdc, and temperature changes can convert the queued nonzero command to zero.
- State updates only after HAL accepts the final packet into the hardware mailbox.

### Boot-only calibration qualification

- Full schema and CRC validation occur in `ecu_pack_current_calibration_qualify()`.
- Runtime model calls use a qualified immutable handle and do not recalculate CRC.

### Persistent transition/tracking state

- Added active raw command extrema, settled anchor, cumulative drift, material-change timestamp, settling duration, and confirmation count.
- Added sign/zero precedence, microstep rate/band limits, anchor-deviation guard, cumulative-drift guard, and deterministic re-anchoring.
- Reversal state survives commanded-zero cycles and waits for physical zero before opposite-sign torque.

### Model completeness

- Added age-derived asymmetric speed uncertainty and Vdc/temperature uncertainty.
- Added union across all touched torque and operating regions with region overflow failure.
- Transition schema now includes physical profile, direction, full span, and operating region.
- Larger-span envelopes must contain smaller-span envelopes and use no shorter settling time.
- Added bounded transition refinement for optional increases.

### Residual monitoring and authority state

- Integrated time-aligned canonical pack-current residual monitoring into runtime CAN service.
- Added source-epoch settling behavior and latched aggregate envelope fault.
- Added `NORMAL`, `LOW`, `TORQUE_EXHAUSTED`, and `ZERO_STEADY_AUX_INFEASIBLE` reporting.

### Runtime diagnostics

- Added a `power` CLI command for clamp reason, authority state, path/sign/phase, model-call counts, predicted interval, residual state, source epoch, calibration qualification, and deadline overruns.
- Documented that residual alignment currently uses the coherent AMS electrical-frame receive time; physical acquisition/publication latency remains a certification input until the CAN protocol carries a sample timestamp.

### WCET contract

- Added source/test limits of 32 steady calls, 8 transition calls, 64 cell evaluations, and 4 refinements.
- The final hardware commit is explicitly limited to zero model calls.
- A defensive execution-count violation commands the static certified zero/decay envelope.

## Test additions

- direct whole-cell infeasibility regression;
- transition refinement;
- reversal-through-zero;
- settling/re-anchor;
- microstep drift;
- late authority and operating-point re-verification;
- zero-steady auxiliary infeasibility;
- transition monotonicity rejection;
- region overflow;
- 20,000 randomized clamp contract cases;
- independent Python/C current-model vectors;
- residual persistence/latching/stale/source-epoch cases;
- live AMS v0.3.6 producer/ECU consumer compatibility.

## Release locks

```text
ECU_AMS_POWER_CLAMP_IMPLEMENTED = 1
ECU_AMS_POWER_CLAMP_VALIDATED   = 0
```

The checked-in calibration has no steady or transition cells and `evidence_valid=false`. No vehicle-authoritative current calibration is included.

## Not claimed

- no GCC or ARM target build was run in the completion environment;
- no target WCET, stack, map, or ISR-interference measurement;
- no hardware, HIL, dyno, APM-primary, or vehicle validation;
- no CM200 timeout/EEPROM verification;
- no final DHAB/APM boundary or auxiliary-current certification.
