# ECU Host Tests

Host-side tests for ECU code that does not require STM32 hardware.

Current coverage:
- AMS telemetry parser bounds checks
- 75s / 5x15 voltage packet layout
- temperature and fan tail packets
- estimator CAN frame raw storage
- stale AMS telemetry detection
- invalid CAN frame rejection
- first-party driver and utility behavior using host HAL/CMSIS mocks

Run from `host_tests/`:

```sh
make CC=gcc clean
make CC=gcc unit
make CC=gcc drivers
make CC=gcc test
make CC=gcc analyze
```

Or run the whole host suite:

```sh
make CC=gcc ci
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

## Driver/utility tests

`make drivers` builds first-party ECU drivers against host HAL/CMSIS mocks. These tests are designed to catch bugs that the AMS-only tests and host regression tests cannot see, including bad float suffix edits, failed HAL status propagation, signed sensor conversion mistakes, and timeout paths.

Coverage includes:

- `map()` edge cases, reversed ranges, and zero-width input range handling
- APPS potentiometer init, clamping/filtering, failure thresholds, and plausibility debounce
- BSE pressure sensor init, percent conversion, failure thresholds, and plausibility debounce
- PWM null handling, init side effects, duty-cycle clamping, and CCR calculation
- flow sensor zero-capture reset, duty/frequency calculation, and null/clock guards
- MPU6050 null/invalid-config handling, init write status propagation, signed big-endian raw conversion, scale factors, temperature conversion, and failed-read no-overwrite behavior
- CAN device init, transmit success, transmit timeout, null guards, and HAL add-message failure propagation
- CLI device init, tokenization, blocking/ISR UART print paths, and status propagation
- dashboard write path, NTC init, and RTC BCD/decimal helper conversions

## Regression tests

`make test` keeps the broader regression harness around AMS frame parsing and integration-style parser behavior.

## Analyzer

`make analyze` runs GCC analyzer over the unit, driver, and regression host-testable sources.
