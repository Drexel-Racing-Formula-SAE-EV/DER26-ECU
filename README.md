# DER26 ECU Firmware

STM32F767ZI + FreeRTOS firmware for the Drexel Electric Racing DER26 vehicle ECU.

Current source revision in this repository: **v2.10.7** (`DER26-ECU-v2.10.7-SAFETY2-20260827`).

The ECU acquires driver and vehicle inputs, supervises the AMS and CM200 inverter, generates a bounded torque request, controls supporting low-voltage functions such as coolant pumping, and records vehicle/CAN data to SD storage.

> Bench profiles intentionally inhibit propulsion authority. A vehicle-authority build requires explicit evidence gates for hardware interfaces, calibration, timing, CAN behavior, watchdog behavior, RTOS memory, and safe outputs.

Start with the [documentation index](docs/README.md).

## Repository map

```text
Core/Src/tasks/        FreeRTOS task entry points and periodic state machines
Core/Src/power/        pack-current model, torque clamp, residual monitor
Core/Src/ext_drivers/  board/device adapters, CAN, AMS, CM200, cooling, logger
Core/Inc/              application interfaces and configuration contracts
Drivers/               STM32 HAL/CMSIS vendor support
Middlewares/           FreeRTOS and STM32 middleware
FATFS/                 FatFs integration for SD logging
host_tests/            host unit/SIL/stress/sanitizer harness
ci/                    repository, architecture, and target-build gates
Tools/                 CAN/log/current-model analysis utilities
docs/                  maintained firmware and bring-up documentation
```

See [Code organization](docs/CODE_ORGANIZATION.md) for a maintainer-oriented map.

## Control and safety boundary

The ECU does **not** implement low-level motor field/current control. The CM200 remains responsible for inverter/motor control and its internal protections. The ECU produces a bounded torque request only after the software authority chain is satisfied.

The high-level path is:

```text
APPS / brake / protected discretes
              |
              v
      driver-intent tasks
              |
              v
     candidate torque request
              |
              +------------------------+
              |                        |
              v                        v
        AMS power authority       CM200 capability/state
              |                        |
              +-----------+------------+
                          v
                 torque/current clamp
                          |
                 final commit-time checks
                          |
                          v
                    bxCAN mailbox
                          |
                          v
                        CM200
```

Nonzero torque must survive fresh/coherent AMS authority, CM200 state/capability, current-model constraints, discrete safety inputs, and final mailbox-commit checks. Loss of required authority removes nonzero torque permission.

Detailed contracts:

- [Torque-to-pack-current clamp](docs/ECU_TORQUE_TO_PACK_CURRENT_CLAMP.md)
- [AMS power protocol](docs/ECU_AMS_POWER_PROTOCOL_V2.md)
- [Torque removal and availability budget](docs/ECU_TORQUE_REMOVAL_AND_AVAILABILITY_BUDGET.md)
- [CM200 CAN contract and bring-up](docs/CM200_CAN_CONTRACT_AND_BRINGUP.md)
- [Safety model](docs/SAFETY_MODEL.md)

## Main application areas

### Driver and vehicle state

`Core/Src/tasks/` contains the APPS, brake, RTD, CAN, cooling, dashboard, CLI, and error-supervision tasks. Input plausibility and authority are kept separate from the final CAN command commit.

### Battery-power supervision

`Core/Src/power/` contains the pack-current model, calibration interface, torque clamp, and current-residual monitor. The ECU consumes canonical AMS power authority rather than branching on the AMS current-source implementation.

### CAN and inverter supervision

The ECU supervises required CM200 feedback, AMS status/power frames, CAN freshness, sequence/coherency, hardware mailbox ownership, and bus error state. v2.10.7 adds the dedicated bxCAN status/error (`CAN1_SCE`) interrupt path so bus-off/error notification handling is not dependent on unrelated RX traffic.

### Cooling

The cooling path acquires coolant temperature/flow/pressure inputs and drives the coolant pump. Manual bench control is available in inhibited validation profiles; automatic/vehicle use remains subject to validation gates.

### Logging

The SD logger records decoded ECU/AMS/CM200 state plus accepted raw CAN traffic without participating in torque authority. See [SD/CAN data logger](docs/ECU_SD_CAN_DATA_LOGGER.md).

## Build profiles

The source supports compile-time profiles in `Core/Inc/ecu_config.h`. Bench-oriented profiles retain sensing, CAN, diagnostics, logging, and validation functionality while inhibiting propulsion outputs. Vehicle authority requires explicit evidence acknowledgements; those macros represent completed validation evidence, not substitutes for it.

See [Current source status](docs/STATUS.md) and [Safety model](docs/SAFETY_MODEL.md).

## Host validation

From the repository root:

```bash
cd host_tests
make CC=gcc ci
```

Useful focused targets include:

```bash
make CC=gcc unit
make CC=gcc drivers
make CC=gcc board-integration
make CC=gcc torque-clamp
make CC=gcc residual-monitor
make CC=gcc power-consumer
make CC=gcc system-sil
make CC=gcc stress
make CC=gcc asan
make CC=gcc ubsan
```

See [Validation strategy](docs/VALIDATION.md), `host_tests/README.md`, and `host_tests/docs/TEST_MATRIX.md`.

## Target build

The checked-in `.project`, `.cproject`, `.ioc`, linker scripts, STM32 support, FreeRTOS, and FatFs integration are retained so the repository can be imported directly into STM32CubeIDE. CI also contains an ARM-GCC headless-build path for build reproducibility and firmware-size checks.

## Hardware and bring-up

- [Pin map](docs/PIN_MAP.md)
- [ECU hardware bring-up](docs/ECU_HARDWARE_BRINGUP.md)
- [BSPD interface and test plan](docs/BSPD_INTERFACE_AND_TEST_PLAN.md)
- [Current-model certification campaign](docs/ECU_CURRENT_MODEL_CERTIFICATION_CAMPAIGN.md)

## Contribution/maintenance notes

The historical `Core/Src/ext_drivers/` name is broader than its current contents. It is intentionally retained because CubeIDE metadata, host tests, and CI source-contract gates reference those paths. A future path refactor should be done incrementally with build/test updates rather than as a cosmetic mass rename.

Repository status, known target-validation gaps, and version notes are kept in maintained documents under `docs/` rather than accumulating one-off patch/review files at the repository root.
