# Code Organization

This map focuses on project-authored code. STM32 HAL/CMSIS, FreeRTOS, FatFs, and other generated/vendor support remain in their standard directories.

## `Core/Src/tasks/`

FreeRTOS task entry points and periodic state machines:

- `apps_task.c` — accelerator sensing and plausibility.
- `bse_task.c` — brake sensing and plausibility.
- `bppc_task.c` — protected inputs / supporting vehicle state.
- `rtd_task.c` — ready-to-drive state machine.
- `canbus_task.c` — CAN RX/TX supervision and final command publication.
- `cool_task.c` — cooling acquisition/control cadence.
- `dashboard_task.c` — dashboard state publication.
- `cli_task.c` — service interface.
- `error_task.c` — fault aggregation/safety supervision.
- `acc_task.c` — supporting acquisition/state path.

## `Core/Src/power/`

Battery/current-constrained torque authority:

- `ecu_pack_current_model.c` — bounded pack-current prediction.
- `ecu_pack_current_calibration.c` — calibration artifact/interface.
- `ecu_torque_clamp.c` — torque/current envelope and clamp state.
- `ecu_current_residual_monitor.c` — measured-current residual supervision.

## `Core/Src/ext_drivers/`

Historical directory name retained for build compatibility. It now contains board-facing adapters and services, including:

- `ams.c`, `ams_power_consumer.c` — AMS protocol/state consumers.
- `cm200.c` — inverter interface and supervision.
- `canbus.c` — bxCAN service and notification handling.
- `ecu_safety.c` — direct safe-output helpers.
- `cooling_control.c`, `flow_sensor.c`, `pressure_sensor.c`, `ntc.c` — cooling/sensor support.
- `sdcard_service.c`, `ecu_data_logger.c` — storage and logger.
- `dashboard.c`, `cli.c` — service/user interfaces.
- `mpu6050.c`, `pwm.c`, `poten.c`, `map.c`, `stm32f767.c` — board/device utilities.

## `Core/Inc/`

Application interfaces, configuration macros, build profiles, type contracts, and matching module headers.

## `host_tests/`

Host-compiled unit/SIL/fault-injection/stress/sanitizer harness. `host_tests/docs/TEST_MATRIX.md` maps major tests to behavior.

## `ci/`

Static/source-contract checks plus the headless STM32 ARM-GCC build. These gates protect structural properties that are easy to regress accidentally, including expected project files, evidence locks, CAN contracts, storage ownership, and source coverage.

## `Tools/`

Offline utilities for CAN-log decoding and other calibration/analysis workflows.

## Why `ext_drivers` is not renamed now

The directory is broader than its name suggests, but CubeIDE metadata, Makefiles, test harnesses, and CI scripts reference the existing paths. A cosmetic mass rename would create a large nonfunctional diff and unnecessary build risk. If reorganized later, migrate modules incrementally with build/test changes in the same commit.
