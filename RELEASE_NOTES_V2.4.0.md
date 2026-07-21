# DER26 ECU Firmware v2.4.0 Release Notes

## CM200 integration

- Decodes 11 CM200 broadcast messages covering temperatures, speed/position, currents, voltages, internal states, POST/RUN faults, command/feedback timer, firmware identity, and torque capability.
- Requires fresh `0x0A5`, `0x0A7`, `0x0AA`, `0x0AB`, `0x0AC`, and `0x0B1` feedback for torque.
- Correlates the inverter's expected rolling counter with transmitted commands and requires observed progression before synchronization.
- Validates commanded-torque echo and detects frozen/backwards controller power-on timer behavior.
- Treats nonzero POST/RUN faults and VSM Fault/Recycle states as immediate controller faults.
- Separates healthy feedback from VSM torque readiness to avoid blocking precharge while still preventing early RTD.
- Adds startup/runtime fault latching, 3 s power sequencing, 5 s feedback grace, torque-capability clamp, and slew limiting.
- Raises command rate from 20 Hz to 100 Hz.

## CAN and stale-command hardening

- Replaces the torque FIFO with a one-slot latest-value mailbox.
- Rechecks every enabled command at the CAN task immediately before transmit.
- Sends an independent disable when APPS stops feeding the mailbox.
- Adds bxCAN list filters for supported AMS/CM200 standard data frames.
- Rejects remote frames and drains up to eight FIFO frames per callback.
- Tracks accepted, ignored, malformed, remote, overrun, recovery, replaced, and dropped traffic.
- Prevents malformed data in one protocol from being hidden by a later valid frame in another.

## AMS and state-machine hardening

- A malformed required compact frame immediately revokes its last-good authorization.
- Adds cell, pack-current, pack-voltage, temperature, average-order, fan, location, and count plausibility checks.
- Fixes RTD to use a momentary start action rather than requiring the button to remain held.
- Consumes button presses made before brake/safety conditions and requires a fresh release/press after every RTD loss.
- Uses a 2 s ready-to-drive sound, 50 Hz RTD evaluation, and debounce-on-healthy TSAL/motor-OK inputs.

## Diagnostics and supervision

- Adds `cm200` detailed diagnostics and `tasks` stack high-water reporting.
- Adds AMS-pack/CM200-bus voltage-delta diagnostics without making an unvalidated hard gate.
- Adds cooling-task heartbeat and stopped-flow freshness.
- Lowers CLI priority below safety-control tasks and prevents a pending command from being overwritten in the UART ISR.
- Adds a required `ECU_CM200_CAN_CONTRACT_VALIDATED=1` vehicle-build acknowledgement.

## Compatibility and required action

- Bench builds remain output-inhibited.
- Vehicle builds now require `ECU_BUILD_PROFILE=1`, `ECU_BSPD_INTERFACE_3V3_VALIDATED=1`, and `ECU_CM200_CAN_CONTRACT_VALIDATED=1`.
- CM200 required-message mask/rates and rolling-counter settings must be configured and captured before vehicle use.
- The new hardware filter intentionally ignores unsupported CAN IDs; add any future receive contract explicitly with tests.
- An ARM target build and all staged hardware tests remain required before flashing a vehicle-release image.
