# ECU Hardware Bring-up

Perform these stages in order. Keep HV, AIR control, inverter phase outputs, and actuator loads physically unable to energize until the stage explicitly requires them. Host tests and a clean target build do not replace electrical evidence.

## Stage 0 — build identity and static evidence

1. Start with `ECU_BUILD_PROFILE=0`.
2. Clean and rebuild the complete CubeIDE project.
3. Review the ELF/map for STM32F767, expected task/source inclusion, memory use, and discarded/retained safety paths.
4. Flash with HV, AIRs, inverter enable, and actuator loads disconnected.
5. Run `ver`; require a v2.5.1 bench image with `inhibited:1`, `bspd_3v3:0`, `cm200_contract:0`, `ams_clamp_impl:0`, and `ams_clamp_valid:0` unless the corresponding test records already close those contracts. The v2.5.1 vehicle profile is intentionally source-locked until the numeric AMS current-to-torque clamp exists.
6. Run `tasks` during every long bench session and retain minimum stack high-water marks.

## Stage 1 — power and output defaults

Use current-limited LV power. Monitor the 12 V, 5 V, and 3.3 V rails and these pins directly:

- `Firmware_Ok` PA7: low.
- `MTR_EN` PF10: low.
- `Cascadia_ON` PA5: low.
- Buzzer PF13: low except during intentional RTD testing.
- Brake light PD14: on if BSE acquisition is invalid.
- Coolant pump: 100% while calibration is open.

Repeat cold start, warm reset, brownout, debugger attach/detach, watchdog reset, and rapid LV power cycling. No critical output may glitch high during GPIO initialization, task creation, exception entry, or reset.

## Stage 2 — protected discrete inputs

Use protected 3.3 V sources only. Never inject 5 V or 12 V into an MCU pin.

| Input | Pin | Firmware semantic | Disconnected default |
|---|---|---|---|
| `BSPD_OK` | PE13 | high = healthy | fault, pull-down |
| `BMS_Fail` | PF15 | high = fault | fault, pull-up |
| `IMD_Fail` | PF14 | high = fault | fault, pull-up |
| `MTR_Fault` | PB2 | high = fault | fault, pull-up |
| `MTR_Ok` | PF12 | low = healthy | not healthy, pull-up |
| `TSAL_HV_SIG` | PC5 | high = TS active | inactive, pull-down |
| RTD button | PE4 | low = pressed | released, pull-up |

For BMS, IMD, BPSD, and motor fault, verify assertion by the next 100 Hz supervisor sample and recovery only after 250 ms continuously healthy. TSAL and motor-OK losses must be immediate; assertion requires three consecutive 50 Hz samples. Verify source voltage, MCU-pin voltage, polarity, leakage, pull strength, open-wire state, and power-off backfeed.

The nominal 12 V BPSD output requires the protected fail-low interface in [the BPSD test plan](BSPD_INTERFACE_AND_TEST_PLAN.md).

## Stage 3 — APPS, BSE, BPPC, and cooling inputs

Record raw ADC counts at mechanical minimum, maximum, and multiple intermediate points. Update constants only from signed-off measurements.

APPS at 100 Hz:

- both channels stay inside electrical range;
- current raw normalized channels agree within the configured threshold;
- a sustained split faults after 100 ms;
- ADC start/conversion/stop failures fault immediately;
- filtered demand starts without a false step;
- an APPS fault produces immediate disable and requires RTD re-entry;
- full pedal produces no more than the calibrated 200 Nm request before CM200 capability limiting.

BSE/BPPC:

- both channels stay inside range and agree;
- ADC3 channel switching returns the correct physical input;
- acquisition failure sets BSE fault and turns on the brake light;
- brake light threshold is measured mechanically;
- brake above 10% plus APPS above 25% latches BPPC;
- APPS below 5% clears BPPC, but does not automatically restore RTD.

Cooling:

- confirm pump duty is 100%;
- verify a stopped flow pulse stream becomes stale and reports zero instead of retaining the last frequency;
- exercise ADC3 mutex/contention failure and cooling-task heartbeat detection;
- do not use temperature values or protection thresholds until the transfer functions are calibrated.

## Stage 4 — CAN and AMS

Use 250 kbit/s CAN and measured 120-ohm end termination. The ECU hardware filter admits the supported AMS IDs and CM200 broadcasts only.

Run:

```text
can
ams
fault
```

Require standard data frames, DLC 8, coherent changing `0x680` sequence, and independently fresh `0x680`, `0x681`, and `0x682`. Verify big-endian scaling and every BMS_OK, inhibit, validity, fault, ADBMS, thermal, and heartbeat flag.

Fault injection:

- remove each required frame for more than 500 ms;
- send a remote frame or wrong DLC for each required ID;
- repeat and jump the status sequence;
- send cell values outside 0.5-5.0 V or inverted min/max;
- send pack current outside the configured plausibility envelope;
- send thermal values outside -40 to 150 C, average outside min/max, or fan above 100%;
- set thermal bits 4, 5, and 7;
- create controlled FIFO overrun, error-passive, bus-off, and recovery cases.

A malformed required frame must revoke authorization immediately; it must not remain trusted for the remainder of its former freshness window.

## Stage 5 — CM200 broadcast and command contract

Keep the inverter mechanically and electrically unable to create torque. Export the CM200 parameter set and close every item in [the CM200 contract](CM200_CAN_CONTRACT_AND_BRINGUP.md).

Run:

```text
can
cm200
status
fault
tasks
```

Verify the required 100 Hz broadcasts `0x0A5`, `0x0A7`, `0x0AA`, `0x0AB`, `0x0AC`, and `0x0B1`. Verify optional 10 Hz temperatures/firmware and 100 Hz current frames separately. Required-frame age must remain below 250 ms.

Capture ECU command `0x0C0`:

- nominal period 10 ms;
- signed little-endian torque, 0.1 Nm/count;
- constant forward direction on enable and disable;
- five disable frames before enable;
- 4-bit rolling counter with 15-to-0 wrap;
- counter advances only on HAL-accepted submissions;
- latest-value behavior: a newer disable replaces an unsent positive request;
- capability clamp follows `0x0B1`;
- positive demand rises at 1000 Nm/s and falls at 2000 Nm/s;
- every fault bypasses slew and immediately sends zero/disable.

Verify the inverter's `0x0AA` expected counter visibly correlates and progresses, `0x0AC` echoes the current/previous command, and its 3 ms power-on timer advances. Freeze/corrupt each signal, inject three post-sync counter/echo mismatches, back-step/reset the timer, and set POST/RUN fault words. Confirm reset-required runtime latching.

Power-sequence capture:

1. Base gates healthy.
2. PF10 `MTR_EN` rises.
3. PA5 `Cascadia_ON` rises 3 s later.
4. Healthy CM200 communications arrive within 5 s.
5. Missing feedback times out, latches, and drops both outputs.

Confirm healthy feedback during precharge can permit `Firmware_OK`, but RTD/torque remain blocked until VSM state 5 or 6 and forward direction are reported. Characterize the diagnostic AMS-pack/CM200-bus voltage delta through precharge; it is not yet a torque gate.

Only after retaining the parameter export, raw traces, test results, and reviewer approval may `ECU_CM200_CAN_CONTRACT_VALIDATED=1` be used.

## Stage 6 — RTD and shutdown

With torque physically inhibited:

1. Assert TSAL and motor-OK for the healthy debounce interval.
2. Observe the RTD button released.
3. Apply mechanical brake and establish every safety/CM200 torque-ready gate.
4. Press the dedicated RTD button.
5. Release the momentary button; the sound must continue.
6. Measure a continuous sound of approximately 2 s.
7. Confirm RTD enters enabled only after the sound interval.
8. Release the brake; RTD should remain enabled.

Pressing/holding RTD before the brake or safety conditions are valid must be consumed and must not auto-arm later. Release and press again after conditions are valid.

Inject each loss separately: TSAL, motor OK, AMS, CM200 required freshness, CM200 VSM torque-ready state, CAN, BMS, IMD, BPSD, APPS, BSE, BPPC, task heartbeat, and controller fault. Require immediate torque disable, RTD exit, a 100 ms `Firmware_OK` trip request where applicable, and a new release/press sequence before re-entry. Releasing the RTD button alone after enable must not exit RTD.

## Stage 7 — exception, scheduling, and watchdog

Using approved instrumented bench hooks, exercise allocation failure, stack overflow, `configASSERT`, NMI, HardFault, MemManage, BusFault, UsageFault, and HAL fatal error. Probe PA7/PF10/PA5; all must go low before the firmware traps.

Suspend/starve each supervised task and verify the heartbeat gate. Stall APPS and confirm the CAN task independently transmits disable after its 25 ms receive wait. Run long CLI output and prove the lower-priority CLI cannot delay the 100 Hz command/error tasks. Finally suspend the error supervisor and measure independent-watchdog reset time and reset-cause reporting.

## Open calibration and release gates

- Coolant temperature transfer functions and fault thresholds.
- Coolant pressure/flow protection policy.
- Vehicle-specific APPS/BSE endpoints and tolerances.
- Real BMS, IMD, BPSD, TSAL, and motor-controller source/pin polarity and voltage.
- CM200 EEPROM/CAN contract and every required frame/fault behavior.
- ARM target clean build, map review, stack margins, timing/CPU load, and warning-free CI.
- EMI, brownout, CAN fault, watchdog, shutdown-loop, brake, and controlled low-torque vehicle tests.

Do not change to the vehicle profile until every applicable item has an owner, date, captured evidence, and reviewer sign-off.
