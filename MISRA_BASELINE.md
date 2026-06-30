# ECU MISRA C Baseline

This branch starts a MISRA C migration for the DER26 ECU firmware. It does not
claim certified MISRA compliance. The first step is a CI-enforced hygiene gate
for first-party firmware files plus low-risk source cleanup.

## Enforced first-party scope

The MISRA-lite CI check covers:

- `Core/Inc/app.h`
- `Core/Inc/board.h`
- `Core/Inc/ext_drivers/*.h`
- `Core/Inc/tasks/*.h`
- `Core/Src/app.c`
- `Core/Src/board.c`
- `Core/Src/stm32f7xx_it.c`
- `Core/Src/ext_drivers/*.c`
- `Core/Src/tasks/*.c`

## Excluded/deviation scope

The following are intentionally excluded from this baseline gate:

- STM32 HAL / CMSIS vendor code
- FreeRTOS middleware
- CubeMX-generated startup/system/main/MSP files
- Linker scripts and startup assembly
- Host tests and CI tooling

These files can still be reviewed separately, but they should not block the
first-party MISRA migration.

## CI errors

`ci/scripts/check_ecu_misra_hygiene.py` fails on:

- UTF-8 BOMs
- hidden/bidirectional Unicode characters
- non-ASCII characters in first-party firmware
- reserved header-guard identifiers such as `__APP_H_`
- empty function parameter lists instead of `(void)`
- non-static FreeRTOS task entry functions
- dynamic allocation calls
- `goto`
- unsafe string calls: `gets`, `strcpy`, `strcat`, `sprintf`

## CI warnings

The same script warns, but does not yet fail, on:

- `atoi`
- `ret |= function_call` style status accumulation
- function-like macros
- unsuffixed decimal floating literals

Warnings are intended as future ratchet targets once the first baseline is
stable.

## Rev 1 cleanup included

- Replaced reserved first-party header guards with project-specific guards.
- Changed empty parameter-list declarations to explicit `(void)`.
- Made FreeRTOS task entry functions file-local with `static` linkage.
- Made CLI command tables and command handlers file-local.
- Replaced `atoi()` in the SSA command with checked `strtol()` parsing.
- Removed status accumulation using `|=` from first-party ECU code.
- Converted selected string interfaces to `const char *`.
- Cleaned MPU6050 driver constants, null checks, status handling, and raw sample sign extension.

## Rev 2 cleanup included

- Added null guards to CLI, dashboard, board, potentiometer, pressure sensor, and STM32 wrapper initialization paths.
- Replaced `strtok()` tokenization with bounded in-place parsing that preserves the existing token-limit behavior.
- Replaced remaining first-party `strlen()` use in ISR-adjacent CLI echo logic with bounded/constant-size checks.
- Converted `map()` from `long double` arithmetic to `float` arithmetic to avoid unnecessary extended-precision math on embedded targets.
- Added explicit `u` suffixes to first-party integer macros used for counts, priorities, CAN IDs, ADC channels, and driver constants.
- Made STM32 mutex attribute objects `static const` to avoid exporting file-local configuration data.
- Initialized RTC local structs before HAL calls and removed a duplicate task include.
- Updated potentiometer/pressure-sensor helpers to store computed percent fields, use `fabsf`, and return explicit `0u/1u` values.
