# ECU Host Tests

Host-side tests for ECU code that does not require STM32 hardware.

Current coverage:
- AMS telemetry parser bounds checks
- 75s / 5x15 voltage packet layout
- temperature and fan tail packets
- estimator CAN frame raw storage
- stale AMS telemetry detection
- invalid CAN frame rejection
- compact AMS `0x680-0x683` frame parsing
- compact BMS_OK/validity/fault torque-gate behavior
- compact rolling-sequence stale/repeat checks
- system SIL fault-injection for AMS torque gate, RTD state machine, CM200 disable-before-enable, and torque-gate matrix
- deterministic fuzz on compact AMS status sequence/fault behavior

Run from `host_tests/`:

```sh
make CC=gcc clean
make CC=gcc unit
make CC=gcc test
make CC=gcc system-sil
make CC=gcc stress
make CC=gcc asan
make CC=gcc analyze
```

## Unit tests

`make unit` builds and runs focused ECU unit tests around the AMS CAN parser and state container. These tests cover:

- initialization/zeroing behavior
- big-endian telemetry decoding
- all 75 cell-voltage slots across 5 segments
- all temperature packet groups, including 17-channel tails
- all fan packet groups, including the 10th-fan tail
- bad header/DLC/null/extended-frame rejection
- raw estimator status packet storage
- stale-CAN timeout behavior, including timer wraparound
- full packet sweep over packet IDs 0 through 61
- compact AMS status/electrical/thermal/health frame decoding
- compact status fault gating and reserved IMD bit behavior
- compact sequence repeat detection and status-frame-only freshness

`make test` keeps the broader regression harness. `make system-sil` runs the ECU software-in-the-loop/fault-injection harness. `make stress` reruns the SIL sequence fuzzer with a longer deterministic cycle count. `make ci` runs unit tests, regression tests, system SIL, and GCC analyzer.
