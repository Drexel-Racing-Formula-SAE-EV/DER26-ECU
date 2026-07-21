# DER26 ECU Firmware v2.4.0

STM32F767ZI / FreeRTOS firmware for the DER26 vehicle ECU. Version 2.4 adds supervised CM200 feedback, closes stale-command and RTD state-machine defects, strengthens AMS/CAN validation, and expands bring-up diagnostics while preserving the output-inhibited default.

## Build locks

Debug, Release, and headless builds default to the bench profile:

```text
ECU_BUILD_PROFILE=0
```

It runs sensing, CAN decoding, command generation, CLI, and safety logic, but holds `Firmware_Ok`, `MTR_EN`, `Cascadia_ON`, and enabled/nonzero torque commands inhibited.

A vehicle build requires all three symbols:

```text
ECU_BUILD_PROFILE=1
ECU_BSPD_INTERFACE_3V3_VALIDATED=1
ECU_CM200_CAN_CONTRACT_VALIDATED=1
```

The BPSD board exports a nominal 12 V active-high `BSPD Ok` signal, while PE13 is a 3.3 V input with no shown level conversion. Never connect that output directly to PE13. The second acknowledgement requires measured CM200 IDs, rates, command mode, rolling-counter behavior, and required broadcasts. These symbols are locks, not proof of hardware validation.

## Safety behavior

- Critical outputs initialize low and are forced low by HAL fatal errors, CPU exceptions, RTOS assertion/allocation/stack failures, and watchdog reset.
- The error supervisor is the only runtime owner of `Firmware_Ok`, `MTR_EN`, and `Cascadia_ON`.
- Vehicle builds use an independent watchdog; APPS, BSE, BPPC, RTD, CAN, and cooling task heartbeats are supervised.
- BMS, IMD, BPSD, motor-fault, TSAL, and motor-OK signals fail closed. Fault assertion is immediate; the principal discrete fault inputs require 250 ms of healthy samples to recover.
- RTD requires TS active, brake applied, healthy controller/safety state, one observed button release, and a new deliberate button press. An early/held press is consumed. The momentary button may be released during the 2 s sound and is not required to remain held after RTD.
- Any RTD gate loss disables torque and requires a new release/press cycle; it cannot automatically re-enter RTD from a held button.
- CLI was moved below safety-control priorities so blocking UART diagnostics cannot starve RTD, CAN, APPS, BSE, or the error supervisor.

## AMS supervision

Torque authorization requires independently fresh `0x680`, `0x681`, and `0x682` frames, coherent status sequence, matching protocol version, BMS_OK/validity flags, zero relevant faults, and sane electrical/thermal values. A malformed required frame invalidates its last good value immediately rather than leaving it trusted until the 500 ms age timeout.

Electrical summaries enforce cell, pack-current, pack-voltage, and min/max plausibility bounds. Thermal summaries enforce -40 to 150 C bounds, `min <= average <= max`, and fan command no greater than 100%. Legacy `0x069` and `0x421` remain diagnostic-only and cannot authorize torque. See [ECU/AMS CAN contract](Core/docs/ECU_AMS_CAN_CONTRACT.md).

## CM200 supervision

The ECU decodes temperatures, speed/position, currents, voltages, internal states, POST/RUN faults, torque/timer, firmware identity, and torque capability from `0x0A0` through `0x0B1`.

Required torque feedback is:

```text
0x0A5  0x0A7  0x0AA  0x0AB  0x0AC  0x0B1
```

Each required frame has a 250 ms timeout. Torque also requires:

- CAN torque mode and no enable lockout;
- zero POST and RUN fault words;
- correlated and progressing inverter expected counter;
- current/previous command echo agreement;
- advancing power-on timer with reset/replay detection;
- VSM Ready or Motor Running state and forward direction;
- fresh positive motoring capability.

CM200 commands now run at 100 Hz. Positive torque is limited by the `0x0B1` capability and calibrated 200 Nm ceiling, with a positive-torque slew limiter. Every fault or inhibit bypasses the down-slew and sends immediate zero/disable. The one-slot transmit mailbox replaces an unsent old request, eliminating FIFO delivery of stale positive torque after a newer disable.

The controller is powered in stages so feedback can start without creating a shutdown/precharge deadlock. Communication health permits `Firmware_OK`; VSM Ready/Running remains mandatory for RTD and torque. Missing startup feedback and runtime feedback/integrity failures are reset-required latches. Details and the EEPROM/test contract are in [CM200 CAN contract and bring-up](Core/docs/CM200_CAN_CONTRACT_AND_BRINGUP.md).

## CAN hardening

- bxCAN hardware list filters admit only the supported AMS and CM200 standard data IDs.
- Remote frames are rejected again in software.
- The RX ISR drains a bounded batch instead of one frame per callback.
- Valid, ignored, malformed, remote, FIFO-overrun, recovery, replaced-command, and dropped-command counts are retained.
- Protocol-specific invalidation prevents one valid subsystem frame from clearing another subsystem's malformed/stale state.
- Command counters advance only after HAL accepts the frame.

## Diagnostics

Useful UART commands:

```text
ver
status
fault
sd
bspd
ams
can
cm200
tasks
throttle
brake
brakelight
```

`ver` prints all build locks. `cm200` reports freshness, states, integrity, fault words, torque capability, firmware identity, temperatures, and a diagnostic AMS-pack/DC-bus voltage comparison. `tasks` reports stack high-water marks.

## Build and validation

Host validation:

```bash
cd host_tests
make CC=gcc clean
make CC=gcc ci
make CC=gcc asan
make CC=gcc ubsan
make CC=gcc stress
```

Bench ARM build:

```bash
ECU_BUILD_PROFILE=0 bash ci/stm32/build_ecu_headless_gcc.sh
```

Vehicle ARM build, only after both hardware contracts are closed:

```bash
ECU_BUILD_PROFILE=1 \
ECU_BSPD_INTERFACE_3V3_VALIDATED=1 \
ECU_CM200_CAN_CONTRACT_VALIDATED=1 \
bash ci/stm32/build_ecu_headless_gcc.sh
```

This revision environment does not provide `arm-none-eabi-gcc`, so a clean team-toolchain target build, map review, flash, and staged hardware validation remain mandatory. Follow [ECU hardware bring-up](Core/docs/ECU_HARDWARE_BRINGUP.md) and [BPSD interface test plan](Core/docs/BSPD_INTERFACE_AND_TEST_PLAN.md).

Coolant temperature conversion and protection thresholds remain uncalibrated. The firmware reports that telemetry invalid and commands the cooling pump to 100% rather than presenting sensor voltage as temperature.
