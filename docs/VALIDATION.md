# Validation Strategy

No single test layer is sufficient for a safety-relevant embedded controller. The repository uses complementary layers.

## 1. Host unit and SIL tests

The host harness exercises hardware-independent and hardware-adapted logic without requiring the STM32 board. Major areas include:

- input/device conversion and driver behavior;
- board initialization;
- SD/FatFs service fault injection;
- AMS power-protocol parsing/coherency;
- CM200/CAN supervision logic;
- pack-current model and torque clamp;
- current residual monitoring;
- system-level SIL state transitions.

Run the full portable suite with:

```bash
cd host_tests
make CC=gcc ci
```

## 2. Deterministic stress

Seeded stress targets exercise long sequences of CAN, sensor, state, storage, and fault combinations while checking invariants. The longer scheduled/manual target extends those cycles to expose wraparound, stale-data, and state-machine boundary issues.

## 3. Sanitizers and static analysis

Host builds provide ASan/UBSan coverage where supported. GCC/Clang analyzers and repository scripts add checks for source structure, task/fault/storage contracts, evidence gates, CAN contracts, and static allocation expectations.

## 4. Headless STM32 build

CI includes an ARM-GCC build path to catch target-only compile/link drift and produce a size report. This complements, but does not replace, the STM32CubeIDE build used by the team.

## 5. Target bench validation

Required for properties the host cannot measure:

- actual Cortex-M7 stack high-water marks;
- WCET and scheduling jitter;
- interrupt latency/interference;
- physical CAN TX/RX/error/bus-off behavior;
- ADC/PWM/timer timing;
- watchdog behavior;
- real sensor polarity/calibration;
- safe GPIO behavior during reset, fault, and power cycling;
- coolant pump/sensor behavior.

## 6. HIL/dyno/vehicle validation

Torque-authority calibrations and performance behavior require the actual electrical/mechanical plant or an appropriate HIL/dyno surrogate. Host PASS does not convert an unvalidated evidence macro into proof.

## Focused commands

```bash
cd host_tests
make CC=gcc unit
make CC=gcc drivers
make CC=gcc board-integration
make CC=gcc sdcard-service
make CC=gcc torque-clamp
make CC=gcc residual-monitor
make CC=gcc power-consumer
make CC=gcc power-integration
make CC=gcc system-sil
make CC=gcc stress
make CC=gcc asan
make CC=gcc ubsan
make CC=gcc analyze
```

See `host_tests/README.md` and `host_tests/docs/TEST_MATRIX.md` for the complete target list.
