# ECU Host Tests

Host-side tests for ECU code that does not require STM32 hardware.

Current coverage:
- AMS telemetry parser bounds checks
- 75s / 5x15 voltage packet layout
- temperature and fan tail packets
- estimator CAN frame raw storage
- stale AMS telemetry detection
- invalid CAN frame rejection

Run from `host_tests/`:

```sh
make CC=gcc clean
make CC=gcc test
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

`make test` keeps the broader regression harness. `make ci` runs unit tests, regression tests, and GCC analyzer.
