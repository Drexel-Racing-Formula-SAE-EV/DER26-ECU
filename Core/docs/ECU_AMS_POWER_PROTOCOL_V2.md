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

`0x689` and `0x68A` are advisory. Corruption invalidates only the corresponding advisory cache and cannot revoke an already valid scalar DCL/CCL bundle.

## Torque behavior in v2.5.1

Compact AMS health (`0x680`-`0x682`) and a fresh protocol-v2 authority bundle are required before an enabled CM200 command can pass. The CAN task first waits for a free bxCAN mailbox, then masks CAN RX, captures a coherent timestamp, re-reads the newest bundle, selects discharge authority for positive torque or charge/regen authority for negative torque, and commits the selected frame to hardware before unmasking RX. A newer zero, fallback, stale bundle, or direction inhibit accepted before that hardware-commit point replaces an older queued command with an explicit disable packet.

The numeric DCL/CCL and power values are not yet converted into a conservative torque ceiling. The vehicle profile therefore requires both a source-owned `ECU_AMS_POWER_CLAMP_IMPLEMENTED` latch and independent `ECU_AMS_POWER_CLAMP_VALIDATED` evidence. The implementation latch is currently `0` and cannot be overridden from compiler flags, so the vehicle profile remains intentionally unbuildable until the stall/low-speed-safe torque-to-DC-current model and clamp exist.
