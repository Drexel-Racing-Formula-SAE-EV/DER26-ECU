# ECU consumption of DER26 AMS power protocol v2

## Required authority bundle

The ECU stages IDs `0x684` through `0x687` atomically. Each frame must be a standard 11-bit data frame with DLC 8, protocol version 2, the same 4-bit rolling counter, and a valid ID-bound CRC-8/SAE-J1850. All four IDs may arrive in any order but only once each within 50 ms.

Authority becomes usable only after two consecutive complete valid bundles. Any malformed required frame, duplicate, skew violation, CRC failure, version failure, semantic failure, or counter discontinuity clears authority. A completed bundle older than 250 ms is unavailable and getters return zeroed outputs.

## Typed API boundary

- `ams_get_immediate_power_authority()` is the only interface intended for the final torque-transmit clamp.
- `ams_get_feasibility_envelope()` exposes exactly `0.1 s`, `10 s`, and `30 s` constant-current feasibility values. They are not a reusable pointwise schedule.
- `ams_get_power_resource_state()` returns synchronized AMS-owned resource data only when `0x689` matches the active bundle and freshness/skew rules.
- `ams_get_power_soh()` exposes synchronized SoH values from the required bundle.
- `ams_encode_mission_request()` builds protocol-v1 mission requests on `0x688`.

`0x689`, `0x68A`, and source-diagnostic `0x68B` are advisory. Corruption invalidates only the corresponding advisory cache and cannot revoke an already valid scalar DCL/CCL bundle.


## Advisory ordering and wrap behavior

Optional `0x689` and `0x68A` frames may arrive before or after the final required
core frame. They are usable when their rolling counter matches the active bundle,
their age is valid, and the shortest modular distance between their receive
timestamp and the active bundle receive timestamp is no greater than 50 ms. This
keeps advisory association order-independent and correct across the 32-bit RTOS
tick wrap. Advisory corruption or excess skew still invalidates only the affected
optional cache.


## Canonical current diagnostic frame `0x68B`

`0x68B` is a compact advisory frame sent after the electrical frame. It carries current source, quality, canonical boundary, source epoch, low 16 bits of sample sequence, and physical sample age. The ECU uses it to count distinct physical current samples and reconstruct the sample timestamp for residual monitoring. It never changes DCL/CCL, direction authorization, or torque-model selection. Once an ECU has observed `0x68B`, stale or incoherent metadata invalidates the residual-measurement input instead of silently falling back to receive time.

## Torque behavior in v2.6.3

Compact AMS health (`0x680`-`0x682`) and a fresh protocol-v2 authority bundle are required before an enabled CM200 command can pass. The APPS task converts the candidate torque into conservative steady and transition pack-current intervals. After any bxCAN mailbox wait, the CAN task takes the newest coherent authority/capability snapshot and re-verifies the cached intervals inside the protected hardware-commit section. It performs no model call and no torque search there. A newer zero, stale bundle, direction inhibit, DCL/CCL tightening, capability change, or operating-point generation change converts the queued nonzero request to an explicit disable packet.

The source-owned `ECU_AMS_POWER_CLAMP_IMPLEMENTED` latch remains `1` in v2.6.3. Vehicle authority remains independently locked by `ECU_AMS_POWER_CLAMP_VALIDATED=0` and by the deliberately invalid checked-in current-model calibration. Calibration, target WCET, CM200 timeout, HIL/dyno, and restricted vehicle evidence are still required.


## Current-model residual policy

The residual monitor compares the canonical AMS current sample against the
source-independent predicted pack-current interval. Persistence advances only
on distinct physical sample sequences. A latched residual is included in both
the APPS torque gate and the CAN task's final protected-commit gate.

No separately calibrated degraded torque cap is approved in this release.
Therefore a latched residual forces a zero/disable command and reports
`ECU_CLAMP_REASON_CURRENT_MODEL_RESIDUAL`. It does not open the shutdown circuit
by itself; shutdown escalation remains a separate release-policy decision.
