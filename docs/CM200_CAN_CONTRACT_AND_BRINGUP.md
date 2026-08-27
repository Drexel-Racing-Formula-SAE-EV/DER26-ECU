# CM200 CAN Contract and Bring-up

This document defines the ECU contract for the Cascadia Motion CM200 using software manual `0A-0163-04` (11 August 2025). It is deliberately stricter than merely seeing CAN traffic: vehicle torque requires fresh feedback, zero active fault words, coherent command correlation, and an operational inverter state.

## Required controller configuration

Before setting `ECU_CM200_CAN_CONTRACT_VALIDATED=1`, measure and record all of the following on the installed controller:

- CAN uses standard 11-bit identifiers with offset `0x0A0`.
- Fast broadcast rate is nominally 100 Hz.
- Slow broadcast rate is nominally 10 Hz.
- `0x0A5`, `0x0A7`, `0x0AA`, `0x0AB`, `0x0AC`, and `0x0B1` are enabled.
- Torque command source is CAN and run mode is torque mode.
- Rolling-counter checking is enabled; do not set its EEPROM debounce/maximum to zero.
- ECU command ID is `0x0C0` and the controller timeout is configured consistently with a 10 ms command period.
- Forward-direction encoding and inverter-enable lockout behavior match the captured command/feedback traces.

The acknowledgement is a build lock, not a substitute for the measurements.

## Decoded broadcasts

All values are little-endian, standard-ID, DLC 8 frames.

| ID | Nominal rate | Decoded content | Torque gate |
|---:|---:|---|---|
| `0x0A0` | 10 Hz | Phase-module A/B/C and gate-driver temperatures | Diagnostic |
| `0x0A1` | 10 Hz | Control-board, RTD1/RTD2, and motor-hotspot temperatures | Diagnostic |
| `0x0A2` | 10 Hz | Coolant, inverter-hotspot, motor temperature, torque shudder | Diagnostic |
| `0x0A5` | 100 Hz | Electrical angle, motor speed, electrical frequency, resolver delta | Required, 250 ms timeout |
| `0x0A6` | 100 Hz | Phase A/B/C and DC-bus currents | Diagnostic |
| `0x0A7` | 100 Hz | DC-bus, output, Vd and Vq voltages | Required, 250 ms timeout |
| `0x0AA` | 100 Hz | VSM/inverter states, command/run mode, enable/lockout, direction, expected rolling counter, limit flags | Required, 250 ms timeout |
| `0x0AB` | 100 Hz | 32-bit POST and 32-bit RUN fault words | Required, 250 ms timeout; any nonzero word blocks |
| `0x0AC` | 100 Hz | Commanded torque, torque feedback, 3 ms power-on timer | Required, 250 ms timeout |
| `0x0AE` | 10 Hz | Project, software version, month/day and year | Diagnostic, 2 s timeout |
| `0x0B1` | 100 Hz | Available motoring and regenerative torque | Required, 250 ms timeout |

Temperature, torque, current, and high-voltage values use the manual's signed 16-bit formats and 0.1-unit scale. Motor speed is signed RPM. The ECU retains the raw project/version/date words because their display convention is controller-project-specific.

## Two readiness levels

The implementation separates two states to avoid a precharge deadlock:

1. **Feedback healthy** permits `Firmware_OK` and the shutdown/precharge sequence to progress. It requires all required broadcasts, command freshness, rolling-counter correlation, torque echo, advancing power-on timer, CAN torque mode, no enable lockout, and zero fault words.
2. **Torque ready** additionally requires VSM state 5 (Ready) or 6 (Motor Running) and the commanded forward direction. RTD and enabled torque packets require this state.

Thus a healthy controller in a precharge state can allow the hardware sequence to continue, but it cannot authorize RTD or torque.

## Command integrity

The ECU sends `0x0C0` every 10 ms:

- bytes 0-1: signed torque, 0.1 Nm/count;
- bytes 2-3: zero speed command;
- byte 4: forward direction, retained during disable;
- byte 5 bit 0: inverter enable;
- byte 5 bits 4-7: 4-bit rolling counter;
- bytes 6-7: zero.

Five disable packets are sent before enable. The transmit path waits for a free bxCAN mailbox, then revalidates every queued enable packet and submits it to hardware while CAN RX is masked. The software queue is a one-slot latest-value mailbox, so a new disable replaces an older unsent torque request; a newer AMS/CM200 inhibit accepted before hardware commit also converts the local candidate to disable.

The counter advances only after a real bxCAN TXOK completion callback, not merely after `HAL_CAN_AddTxMessage()` accepts the mailbox request. If the 4 ms completion deadline expires, the ECU requests abort and briefly reconciles a racing late TXOK/abort callback; unresolved software mailbox ownership blocks every later enqueue. This prevents a hardware-free mailbox from being reused while an older command still has ambiguous software ownership. `0x0AA` must track either the next expected count or the one-command-lag count, and the value must visibly progress before synchronization is declared. Initial mismatch is allowed for one acquisition sweep because a controller can retain a previous sender's expected count. After synchronization, three consecutive mismatches are a fault.

`0x0AC` commanded torque must match the current or immediately preceding ECU command. Three consecutive mismatches after initial synchronization are a fault. Its power-on timer must advance; five repeated values or a non-wrap backwards jump is treated as a controller reset/replay.

## Startup and runtime policy

The controller-power outputs use a staged policy:

1. Base ECU/AMS/discrete/CAN gates become healthy.
2. `MTR_EN` asserts.
3. After 3 s, `Cascadia_ON` asserts.
4. CM200 feedback has 5 s from `Cascadia_ON` to become healthy.
5. Failure to become healthy latches a startup fault and drops both outputs.
6. Once feedback has been healthy, loss of a required frame, integrity failure, controller reset, VSM fault/recycle state, or nonzero fault word latches a runtime fault and drops outputs.

The latches require an MCU reset. A loss of torque-ready state without a communication/fault failure immediately exits RTD and disables torque, but does not itself manufacture a communication fault.

## Torque limiting and diagnostics

Positive torque is limited to the smaller of the APPS request, the ECU's 200 Nm calibration ceiling, and `0x0B1` motoring capability. A 1000 Nm/s rise and 2000 Nm/s fall slew limit is applied at 100 Hz. Safety/inhibit transitions bypass the slew limiter and issue immediate zero/disable.

The ECU also compares fresh AMS pack voltage with fresh CM200 DC-bus voltage using a 20 V diagnostic tolerance. This is intentionally non-gating until it is characterized through precharge and on the assembled HV topology.

Use:

```text
ver
can
cm200
status
fault
tasks
```

`cm200` reports required-frame freshness, DC bus, speed, torque command/feedback/capability, VSM and inverter state, counter/echo synchronization, POST/RUN fault words, timer state, temperatures, firmware identity, and the AMS/DC-bus voltage comparison.

## Bench test matrix

With torque mechanically and electrically impossible:

- Capture `0x0C0` period, layout, disable sequence, direction, and counter wrap.
- Confirm hardware filters accept only the documented AMS and CM200 IDs.
- Remove each required broadcast independently for more than 250 ms.
- Disable each required broadcast in the CM200 parameter set one at a time.
- Freeze `0x0AA` expected counter, inject a three-count mismatch, and test wrap 15 to 0.
- Corrupt the `0x0AC` command echo and freeze/back-step the power-on timer.
- Set every POST and RUN fault bit individually using approved controller test methods.
- Exercise VSM startup, precharge, Ready, Running, Fault, Shutdown, and Recycle states.
- Capture the 3 s power delay and 5 s feedback-acquisition timeout.
- Disconnect CAN, remove termination, force no-ACK/error-passive/bus-off, and create controlled RX overload.
- Verify every failure produces a disable command, exits RTD if active, and never leaves an old positive command queued.

Do not enable vehicle outputs merely because the CLI decoder looks plausible. Retain raw CAN captures, controller parameter exports, ECU logs, and the target ELF/map as test evidence.
