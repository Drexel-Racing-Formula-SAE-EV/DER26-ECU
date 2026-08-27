# DER26 ECU Host Validation Suite

The host suite executes safety-relevant ECU logic on Linux with deterministic fake HAL, FreeRTOS queue, and FatFs layers. It complements the STM32 ARM-GCC build; it does not replace target timing, electrical, HIL, or vehicle validation.

## One-command validation

From `host_tests/`:

```sh
make clean
make -j2 ci
```

`make ci` runs the normal contract, regression, driver, board-integration, storage, power-protocol, SIL, stress, evidence-gate, and GCC analyzer targets.

For sanitizer and Clang analysis:

```sh
make asan
make ubsan
make CLANG=clang clang-analyze
```

For extended deterministic stress:

```sh
make stress-long
```

That target runs 250,000 system fault/fuzz iterations and 2,000,000 cross-module cycles. The normal `cross-stress` target runs 200,000 cycles. Sanitizer jobs also run a 25,000-cycle cross-module pass.

## Coverage layers

- **Focused unit/regression:** AMS compact and legacy frames, CM200 supervision, torque authority, current model, residual monitor, timing boundaries, and protocol vectors.
- **Device drivers:** map conversion, APPS potentiometers, pressure sensing, PWM, flow capture, NTC setup, CLI, dashboard UART, MPU6050, and CAN.
- **Board integration:** executes production `board_init()` against fake STM32 handles and verifies every configured channel, timer, UART, I2C device, and CAN startup result.
- **Storage fault injection:** executes production `sdcard_service.c` against scripted FatFs/disk responses for no-card, geometry, mount, short write, corrupt readback, and soak interruption paths.
- **System SIL:** RTD sequencing, BSPD and discrete recovery, AMS/CM200 faults, stale data, malformed/random frames, heartbeat, torque slew, and wraparound behavior.
- **Cross-module stress:** randomized analog inputs, NaN/Inf PWM requests, timer captures, torque-gate combinations, CAN latest-value behavior, CM200 counters, and AMS parser invariants.
- **Static/evidence gates:** all 40 `Core/Src` files must have an explicit host/static/target-build coverage classification; all ten tasks must remain static and startup-gated; CPU fault handlers must force safe outputs; SD and vehicle evidence contracts remain fail-closed.

The suite currently contains 79 named test functions plus protocol probes, differential vectors, integration runners, and deterministic stress loops. See [TEST_MATRIX.md](docs/TEST_MATRIX.md).

## Important target boundaries

The following still require the ARM target build and/or hardware: generated HAL/MSP startup, actual ADC sampling, timer capture electrical behavior, UART/CAN transceivers, SD-card signal/power behavior, interrupt priority/timing, task WCET, stack margins, watchdog operation, and physical safe-output verification.

The GitHub workflow runs host CI, sanitizers, GCC and bounded Clang analyzers, a headless ARM-GCC build, and a weekly/manual long-stress job.
