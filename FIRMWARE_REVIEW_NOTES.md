# ECU Firmware Review Notes — v2.4.0

Review sources included the v2.3 ECU release, current AMS compact CAN contract and firmware references, Cascadia Motion manual `0A-0163-04`, ECU/MCU-breakout/shutdown/BPSD/RTM schematics and BOMs, and comparison against the two public VCU repositories previously identified by the team.

## Closed in this revision

- Added pure, host-tested CM200 decoding for `0x0A0`, `0x0A1`, `0x0A2`, `0x0A5`, `0x0A6`, `0x0A7`, `0x0AA`, `0x0AB`, `0x0AC`, `0x0AE`, and `0x0B1`.
- Added independent required-frame freshness, sanity, fault-word, command-mode, VSM, direction, enable-lockout, rolling-counter, torque-echo, capability, and power-on-timer supervision.
- Split CM200 feedback health from torque-ready state so controller communications can release the shutdown/precharge sequence without authorizing torque early.
- Added 3 s staged controller power-up, 5 s feedback acquisition, and reset-required startup/runtime CM200 fault latches.
- Raised CM200 command/APPS and error supervision to 100 Hz.
- Replaced the eight-deep torque FIFO with a one-slot latest-value mailbox and revalidated enabled packets immediately before transmission.
- Added CM200 motoring-capability clamp, positive-torque slew limit, and immediate fault bypass to zero/disable.
- Added hardware CAN list filters, remote-frame rejection, bounded FIFO draining, and accepted/ignored/malformed/remote/replacement counters.
- Fixed protocol coupling where a valid frame from one subsystem could clear a generic receive fault caused by another; malformed required frames now invalidate their own last-good state.
- Strengthened AMS cell/current/voltage/thermal/average/fan/location/count plausibility.
- Corrected RTD behavior for a momentary start button, consumed early/held presses, required a new action after any RTD loss, and moved to a 2 s sound with 50 Hz state evaluation.
- Added healthy-debounce for TSAL and motor-OK without delaying fault/loss assertion.
- Added cooling-task heartbeat supervision and stopped-flow freshness handling.
- Lowered blocking CLI work beneath safety-control task priorities and preserved pending UART commands from overwrite.
- Added CM200, CAN, stack-margin, and AMS/DC-bus cross-check diagnostics.
- Added a second vehicle-build lock requiring CM200 parameter/traffic validation.

## Intentionally not implemented

- Regenerative torque commands. Regen capability is decoded, but pedal/brake blending, rear-axle stability, AMS charge SoP, inverter/motor limits, and hydraulic fallback require a separately reviewed control contract.
- A hard AMS-pack versus CM200-DC-bus voltage gate. The delta is logged with a 20 V diagnostic tolerance until precharge and installed-topology behavior are measured.
- Closed-loop coolant control or temperature protection using the uncalibrated sensors.
- Runtime DBC parsing, heap allocation, or dynamic CAN registration. Supported IDs and layouts remain compile-time and bounded.
- Automatic clearing of CM200 startup/runtime latches. An MCU reset and investigation are required.

## Hardware/release blockers

- Protected fail-low 12 V `BSPD Ok` to 3.3 V PE13 interface must be installed and measured.
- CM200 standard IDs, rates, active-message mask, CAN torque mode, rolling-counter checking, direction, lockout, fault words, and timer behavior must be captured on the installed controller.
- APPS/BSE endpoints and coolant sensor transfer functions require measured calibration.
- BMS, IMD, BPSD, TSAL, motor-fault, and motor-OK source/pin polarities and levels require bench evidence.
- No ARM target toolchain or hardware is available in this review environment; target ELF/map, timing/load, stack margins, physical CAN, watchdog, shutdown-loop, precharge, and low-torque tests remain mandatory.

These blockers are enforced by the inhibited default and the BPSD/CM200 vehicle-build acknowledgements where firmware can enforce them. Independent hardware safety systems remain authoritative.
