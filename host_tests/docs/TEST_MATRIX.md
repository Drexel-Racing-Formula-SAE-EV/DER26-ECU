# ECU Host Test Matrix

| Layer | Primary targets | Production code exercised | Main faults/invariants |
|---|---|---|---|
| AMS/CM200 unit and regression | `unit`, `test` | `ams.c`, `ams_power_consumer.c`, `cm200.c`, `ecu_safety.c` | Frame bounds, endian decoding, freshness, sequence/counter checks, malformed-frame rejection, authority revocation |
| Current model/clamp | `torque-clamp`, `rev7-invariants`, `current-model-differential` | pack current model and torque clamp | Cell-aligned search, reversal zero gate, transition refinement, stale/invalid input, model call bounds, independent Python/C vectors |
| Residual and availability | `residual-monitor`, `bundle-availability`, `elapsed-timer` | residual monitor and time guards | Persistence only on distinct physical samples, source epochs, exact timeout boundaries, dropped-bundle outage budget |
| Device drivers | `drivers` | map, poten, pressure, PWM, flow, NTC, CLI, dashboard, MPU6050, CAN | Null/invalid inputs, finite bounds, HAL failures, channel validation, UART/I2C transactions, CAN mailbox/filter behavior |
| Board wiring | `board-integration` | production `board_init()` plus drivers | Correct ADC channels, calibrations, timers/CCR channels, UART/I2C/CAN handles, failed CAN start propagation |
| SD service | `sdcard-service` | production `sdcard_service.c` | No card, invalid sector geometry, mount/unmount, short writes, corrupt readback, open failure, soak first-failure stop |
| Protocol compatibility | `power-consumer`, `power-integration`, `power-golden` | AMS protocol-v2 consumer and ECU authority path | Locked vectors, producer/consumer decoding, direction authority, fail-closed integration |
| System SIL | `system-sil`, `stress-system` | AMS/CM200/safety state logic | RTD sequencing, BSPD/discretes, heartbeat, random frames, wraparound, immediate disable and torque slew |
| Cross-module stress | `cross-stress`, `stress-long` | analog/PWM/flow/CLI/CAN/AMS/CM200/safety combination | Deterministic randomized invariants across 200k normal or 2M long cycles |
| Sanitizers | `asan`, `ubsan` | all major host-executed groups | Address, bounds, lifetime, integer/undefined behavior; includes 25k cross-module cycles |
| Static analyzers | `analyze`, `clang-analyze` | host-testable production and tests | GCC analyzer plus Clang analyzer with bounded path-node budget |
| Coverage/evidence gates | manifest/task/fault/SD/profile/static gates | all 40 `Core/Src` files and configuration | No unclassified source, ten static tasks, safe fault handlers, FAT32/no-card contract, vehicle evidence locks |
| Target build | GitHub `stm32-headless-build` | complete STM32 source, FatFs, FreeRTOS, HAL/CMSIS | ARM compile/link and firmware size limits |

## Determinism

Randomized tests use fixed seeds and print the final seed. A failure can therefore be reproduced exactly. Stress counts can be overridden at compile time with `ECU_HOST_LONG_FUZZ_CYCLES` and `ECU_CROSS_STRESS_CYCLES`.

## Explicitly outside host proof

Host results do not certify electrical pin mapping, ADC reference/calibration, timer clock rates, CAN bus loading, SD-card current transients, interrupt latency, FreeRTOS scheduling under target load, watchdog servicing, or physical output de-energization. Those remain target/HIL/bench release evidence.
