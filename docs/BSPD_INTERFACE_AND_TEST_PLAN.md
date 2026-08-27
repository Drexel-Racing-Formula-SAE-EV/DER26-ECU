# BPSD Interface and Test Plan

## Resolved signal contract

The standalone BPSD schematic names its output `BSPD Ok`. The output-stage supply is the 12 V rail, so a healthy high must be treated as a nominal 12 V signal until measured otherwise. The ECU MCU-breakout schematic exposes PE13 with ESD protection but no documented 12 V-to-3.3 V level conversion.

Firmware therefore defines:

```text
PE13 net:        BSPD_OK
healthy:         logic high at the MCU-side 3.3 V interface
fault:           logic low, disconnected, or unpowered interface
input pull:      pull-down
fault assertion: immediate
fault recovery:  25 consecutive healthy samples at 100 Hz (250 ms)
```

The name change is deliberate. Treating this signal as active-high `BSPD_Fail` would invert the safety meaning.

## Mandatory external interface

Do not connect the BPSD board output directly to PE13. The interface must:

- accept the measured BPSD output high voltage and worst-case transients;
- limit the MCU pin to its permitted 3.3 V logic range under normal and single-fault conditions;
- produce a definite low when the BPSD, cable, or interface supply is absent;
- preserve a common reference only where the electrical design permits it;
- avoid back-powering the MCU through PE13;
- meet the vehicle grounding, isolation, EMC, and connector requirements.

Possible implementations include an appropriately rated transistor/open-drain stage, comparator, optocoupler/digital isolator, or a reviewed divider plus protection network. Select and calculate the actual circuit with the electrical team; firmware does not make an unreviewed interface safe.

## Low-voltage interface test

Keep HV, AIRs, inverter enable, and tractive loads disconnected. Use the bench ECU profile.

1. Power the MCU breakout from current-limited LV power.
2. Leave the BPSD interface disconnected. `bspd` must report raw `0`, decoded fault `1`.
3. Drive the interface input with the BPSD output simulator while monitoring both sides on an oscilloscope.
4. Confirm the PE13 waveform never exceeds the MCU limits and has a definite low level when disconnected.
5. Hold the healthy state. The fault must remain asserted until 250 ms of continuous healthy 100 Hz samples has elapsed.
6. Remove the healthy state. The fault must assert on the next supervisor sample (10 ms maximum task period, excluding scheduling jitter).
7. Repeat with BPSD supply loss, open signal wire, open ground/reference where safe, and a short-to-ground.
8. Verify `Firmware_Ok`, `MTR_EN`, `Cascadia_ON`, and CM200 enable remain low in the bench profile.

## Functional BPSD tests

Use calibrated/simulated brake-pressure and current-sensor inputs at the BPSD board. Record raw input voltages, BPSD comparator/timer nodes where accessible, the 12 V `BSPD Ok` output, the conditioned PE13 voltage, CLI output, and elapsed time.

| Case | Expected BPSD output | ECU result |
|---|---|---|
| No brake, zero current | healthy high | clears after recovery debounce |
| Brake only | healthy high | no BSPD fault |
| Current only | healthy high | no BSPD fault |
| Brake + current below timing threshold | healthy during allowed interval | no early ECU fault |
| Brake + current sustained beyond board delay | low | immediate decoded fault |
| Either sensor outside board fault window | low | immediate decoded fault |
| BPSD power removed | low through fail-low interface | immediate decoded fault |
| Signal cable opened | low through PE13 pull-down | immediate decoded fault |
| Output restored after trip | high | 250 ms healthy recovery before clear |

The schematic indicates an approximately 400 ms hardware plausibility timer. Measure the actual trip interval across voltage, component tolerance, and temperature; do not certify it from nominal RC values alone.

## Vehicle-profile release gate

Only define:

```text
ECU_BUILD_PROFILE=1
ECU_BSPD_INTERFACE_3V3_VALIDATED=1
ECU_CM200_CAN_CONTRACT_VALIDATED=1
ECU_AMS_POWER_CLAMP_VALIDATED=1
```

after the interface schematic is reviewed, assembled, electrically measured, fault-injected, and documented; after the independent CM200 contract is validated; and after the implemented conservative low-speed/stall-safe AMS DCL/CCL torque clamp is calibrated and validated. The source-owned `ECU_AMS_POWER_CLAMP_IMPLEMENTED` latch is already enabled by v2.6.1 and cannot be supplied from build flags. These acknowledgements are build interlocks, not automated proof.
