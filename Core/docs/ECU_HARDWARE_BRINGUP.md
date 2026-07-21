# ECU Hardware Bring-up

## Stage 0 — build and static evidence

1. Start with `ECU_BUILD_PROFILE=0`.
2. Clean the CubeIDE project completely and rebuild.
3. Inspect the compiler command or map strings to confirm the bench profile.
4. Confirm no duplicate or contradictory profile symbols are present.
5. Flash with HV, AIRs, inverter enable, and actuator loads disconnected.
6. Run `ver`; require `profile:bench outputs_inhibited:1`.

## Stage 1 — power and output defaults

Use current-limited LV power and monitor the 12 V, 5 V, and 3.3 V rails.

- `Firmware_Ok` PA7: low.
- `MTR_EN` PF10: low.
- `Cascadia_ON` PA5: low.
- Buzzer PF13: low except during an intentional RTD test.
- Brake light PD14: controlled by BSE; a BSE acquisition fault commands it on.
- Coolant pump: 100% default command while calibration is open.

Reset repeatedly and test brownout/power cycling. No critical output may glitch high during reset, task creation, or fault handling.

## Stage 2 — discrete inputs

Use protected 3.3 V test sources. Do not inject 5 V or 12 V into MCU pins.

| Input | Pin | Firmware semantic | Disconnected default |
|---|---|---|---|
| `BSPD_OK` | PE13 | high = healthy | fault (pull-down) |
| `BMS_Fail` | PF15 | high = fault | fault (pull-up) |
| `IMD_Fail` | PF14 | high = fault | fault (pull-up) |
| `MTR_Fault` | PB2 | high = fault | fault (pull-up) |
| `MTR_Ok` | PF12 | low = healthy | not healthy (pull-up) |
| `TSAL_HV_SIG` | PC5 | high = TS active | inactive (pull-down) |
| RTD button | PE4 | low = pressed | released (pull-up) |

For BMS, IMD, BPSD, and motor fault signals, verify immediate fault assertion and five-sample healthy recovery. Confirm the physical circuit can override the internal pull in both states.

## Stage 3 — APPS and brake sensors

Record raw ADC counts at mechanical minimum, maximum, and intermediate points. Update calibration constants only from measured data.

APPS checks:

- both channels inside electrical range;
- raw, unfiltered channel agreement within threshold;
- sustained split trips after the configured plausibility interval;
- either ADC conversion failure trips immediately;
- torque packet remains disabled during any failure;
- moving-average startup does not create a false torque step.

BSE checks:

- both pressure channels inside range;
- dual-sensor agreement;
- ADC3 channel switching returns the expected source each time;
- acquisition/channel failure sets BSE fault and brake light;
- brake percentage and physical brake-light threshold are measured.

Then test BPPC: brake above 10% with APPS above 25% must latch the plausibility fault; APPS below 5% clears it.

## Stage 4 — CAN and AMS

Use 250 kbit/s CAN with correct termination. Run:

```text
can
ams
fault
```

Require:

- standard DLC-8 frames `0x680`, `0x681`, and `0x682`;
- coherent changing `0x680` sequence;
- all three required frames independently fresh;
- correct big-endian AMS values;
- no AMS or ADBMS diagnostic/heartbeat fault;
- thermal fatal/invalid bits clear;
- CAN FIFO overrun count zero;
- CAN hardware/RX/TX fault zero.

Remove each required AMS frame independently for more than 500 ms and confirm torque authorization drops. Repeat sequence, jump sequence, corrupt DLC, set each AMS status fault bit, invert min/max data, set fan command above 100%, and set thermal bits 4, 5, and 7.

## Stage 5 — CM200 command capture

Keep the inverter mechanically and electrically unable to produce torque. Capture CAN ID `0x0C0`.

- Period: nominal 50 ms and always below the controller timeout.
- Bytes 0-1: signed little-endian torque in 0.1 Nm/count.
- Byte 4: constant configured direction, including disable frames.
- Byte 5 bit 0: enable only after the five disable-unlock frames and every safety gate.
- Byte 5 bits 4-7: 0-15 rolling counter with wrap.
- Counter advances only for accepted transmissions.
- Any safety fault produces zero torque and disable.

Inject CAN disconnect, no-ACK, error passive, bus-off, and RX overflow conditions in a controlled bench setup. Require fault reporting and no enable.

## Stage 6 — RTD and shutdown

With torque physically inhibited, exercise the RTD state machine:

1. TSAL active.
2. RTD button observed released once.
3. Brake light active, inverter OK, all faults clear.
4. Press RTD.
5. Buzzer remains active for 3 s.
6. RTD enters enabled only after the full interval.

Remove TSAL, release RTD, or inject every gate fault separately. Each must exit RTD. Confirm the error task, not the RTD task, owns the resulting 100 ms firmware-shutdown pulse.

## Stage 7 — exception, scheduler, and watchdog tests

On an instrumented bench image, deliberately exercise the approved test hooks for allocation failure, stack overflow, assertion, and CPU exception. Observe PA7/PF10/PA5 directly; all must go low before the firmware traps.

For the vehicle profile, suspend or starve each supervised task and confirm the heartbeat fault blocks outputs. Then suspend the error supervisor and confirm the independent watchdog resets the MCU in the measured timeout window. Inspect reset cause after reboot.

## Open calibration gates

- Coolant thermocouple transfer function is not implemented.
- Coolant pressure/flow thresholds and protection policy need calibration.
- APPS/BSE calibration constants require vehicle measurements.
- The MPU6050 is diagnostic only and must be checked for installed address/orientation.
- Real BMS, IMD, BPSD, TSAL, and motor-controller polarities must be measured at both source and MCU pins.

Do not treat successful host tests or a clean ELF/map as evidence that these physical gates are closed.

