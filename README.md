# DER26 ECU Firmware v2.3.0

STM32F767ZI / FreeRTOS firmware for the DER26 vehicle ECU. This revision is a safety-focused integration update based on the current AMS compact CAN contract, CM200 controller manual, ECU/MCU-breakout schematics, shutdown design, and standalone BPSD design.

## Read this before building

The default build is a **bench profile**. It runs sensing, CAN, diagnostics, and host-tested safety logic, but it holds `Firmware_Ok`, `MTR_EN`, `Cascadia_ON`, and nonzero torque requests inhibited.

```text
ECU_BUILD_PROFILE=0
```

The vehicle profile is intentionally compile-locked until the BPSD output has a validated protected interface to PE13:

```text
ECU_BUILD_PROFILE=1
ECU_BSPD_INTERFACE_3V3_VALIDATED=1
```

The BPSD board exports a nominal 12 V active-high `BSPD Ok` signal. The MCU breakout schematic shows PE13 connected without a level shifter. **Never connect that 12 V output directly to PE13.** Use an approved fail-low 3.3 V interface, validate it electrically, then set the acknowledgement symbol. See [BSPD interface and test plan](Core/docs/BSPD_INTERFACE_AND_TEST_PLAN.md).

CubeIDE Debug and Release configurations explicitly default to `ECU_BUILD_PROFILE=0`. Perform a full clean build after changing profiles. The headless build accepts the same values as environment variables.

## Key safety behavior

- All shutdown and inverter-control outputs initialize low.
- FreeRTOS stack overflow, allocation failure, `configASSERT`, HAL fatal error, NMI, HardFault, MemManage, BusFault, and UsageFault paths force critical outputs low.
- The error task is the sole runtime owner of `Firmware_Ok`, `MTR_EN`, and `Cascadia_ON`.
- Vehicle builds use an approximately 2 s independent watchdog. Only the error supervisor refreshes it.
- APPS, BSE, BPPC, RTD, and CAN task heartbeats are supervised.
- Discrete faults assert immediately and require five healthy 20 Hz samples before clearing.
- PE13 is decoded as active-high `BSPD_OK`, not active-high failure. Its pull-down makes a disconnected protected interface fail closed.
- BMS fail, IMD fail, motor fault, and motor-OK inputs use fail-closed pull configuration matching their configured polarity.
- RTD loss requests a 100 ms firmware-shutdown pulse without allowing two tasks to race the output.
- Sensor ADC start, channel-select, conversion, and stop failures propagate into the corresponding safety fault.
- Brake light turns on if the dual brake-sensor path is invalid.

## AMS integration

New torque authorization requires all of the following:

- Fresh, coherent `0x680` status sequence.
- Fresh `0x681` electrical summary and `0x682` thermal summary, each no older than 500 ms.
- Matching compact protocol version.
- `BMS_OK` true and inhibit/fault/invalid/heartbeat flags clear.
- Sane min/max electrical and thermal summaries.
- Thermal overtemperature, severe-overtemperature, and invalid/read-fault flags clear.

Legacy `0x069` and estimator `0x421` frames remain visible for compatibility but cannot authorize torque. Details are in [the ECU/AMS CAN contract](Core/docs/ECU_AMS_CAN_CONTRACT.md).

The STM32 HAL peripheral wrappers now keep pointers to the real Cube-generated handles rather than copying them. This is essential for CAN: the receive callback now compares against the same global `hcan1` handle used by the IRQ.

## CM200 command behavior

CAN ID `0x0C0` is emitted at 20 Hz with the manual-defined little-endian layout.

- Torque is signed and encoded in 0.1 Nm/count.
- Five disable commands precede any enable command.
- Disable packets retain the configured forward direction to avoid a direction-change lockout during a fault transition.
- Byte 5 bits 4-7 carry a rolling counter.
- The counter advances only after HAL accepts the frame for transmission.
- CAN error warning, error-passive, bus-off, last-error-code, and FIFO-overrun conditions are recorded and fail the torque gate.

## CLI

UART CLI commands relevant to bring-up:

```text
ver
status
fault
sd
bspd
ams
can
throttle
brake
brakelight
```

`ver` prints the active build profile, output-inhibit state, and BPSD-interface acknowledgement. Check this banner after every flash.

## Build and validation

Host validation:

```bash
cd host_tests
make CC=gcc clean
make CC=gcc ci
make CC=gcc asan
make CC=gcc stress
```

Bench ARM build:

```bash
ECU_BUILD_PROFILE=0 bash ci/stm32/build_ecu_headless_gcc.sh
```

Vehicle ARM build, only after the BPSD interface is validated:

```bash
ECU_BUILD_PROFILE=1 \
ECU_BSPD_INTERFACE_3V3_VALIDATED=1 \
bash ci/stm32/build_ecu_headless_gcc.sh
```

The current environment used for this revision did not contain `arm-none-eabi-gcc`; both bench and vehicle source profiles were still checked with strict full-application C syntax passes. Build and inspect the actual ELF/map in CubeIDE or CI before flashing.

## Hardware validation status

Firmware logic and host fault injection do not prove PCB polarity, voltage levels, ADC calibration, real CAN timing, watchdog reset timing, braking behavior, or inverter behavior. The default profile remains output-inhibited for that reason.

Use [the hardware bring-up guide](Core/docs/ECU_HARDWARE_BRINGUP.md) and close every gate before changing to the vehicle profile. Coolant thermocouple conversion remains uncalibrated; the firmware reports its temperature telemetry invalid and commands the pump to 100% rather than presenting sensor voltage as degrees Celsius.

