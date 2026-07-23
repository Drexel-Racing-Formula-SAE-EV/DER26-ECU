> **Superseded by `ECU_AMS_V2_SECOND_CLEAN_PASS_REPORT.md` and ECU v2.5.1.** This file is retained as the v2.5.0 first-pass record.

# DER26 ECU AMS v2 compatibility clean pass

Date: 2026-07-22

## Result

The stale ECU-side AMS SoP interface was replaced with the current AMS v0.3.4 protocol-v2 consumer. The implementation is fail-zero, counter/CRC/freshness protected, and separates scalar authority from diagnostic horizons and resource metadata.

## Fixed integration gaps

1. ECU previously filtered and decoded only `0x680`-`0x683`; it now accepts `0x684`-`0x687`, `0x689`, and `0x68A`.
2. ECU previously had no dynamic DCL/CCL authority gate; it now requires a fresh two-good-bundle scalar authority and re-checks it at CM200 transmit time.
3. The three-horizon wire format is represented as three slots only (`0.1/10/30 s`).
4. Optional resource/binding frame corruption is contained without revoking scalar authority.
5. A malformed required power frame immediately revokes authority instead of leaving an older payload usable.
6. Vehicle builds are blocked until the still-missing numeric DCL/CCL-to-torque clamp is independently validated.
7. CAN hardware filters were expanded from five to seven 16-bit list banks to include the new IDs.
8. Error-task reads of the ISR-owned AMS consumer are protected by a critical section.

## Deliberately not implemented

- MPC.
- Numeric current/power-to-torque clamp.
- Low-speed/stall current model.
- Automatic mission-profile transmission task.
- Use of short-horizon envelope values as control authority.

These remain separate work because implementing them without a conservative torque-current model would create a false claim of DCL compliance.
