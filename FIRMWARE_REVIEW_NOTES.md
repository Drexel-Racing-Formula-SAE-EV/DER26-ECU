# ECU Firmware Deep Review Notes

Review focus: logic errors, undefined behavior, unsafe concurrency, parser bounds, task interactions, and hardware-adjacent failure modes.

## High-confidence fixes applied

1. **APPS/CAN TX packet race fixed**
   - `apps_task` now builds a local `canbus_packet_t` and copies it into `data->board.canbus.tx_packet` inside a critical section.
   - This avoids concurrent partial updates while `canbus_task` copies the packet for transmission.
   - Added a `canbus_task != NULL` guard before `xTaskNotify`.

2. **CAN device initialization hardened**
   - `canbus_device_init` now returns early on null `dev`, `hcan`, or `tx_header`.
   - `HAL_CAN_Start` result is explicitly ignored instead of silently unhandled.

3. **CAN mailbox wait no longer tight-spins**
   - `canbus_transmit` now calls `taskYIELD()` while waiting for a free mailbox.
   - Timeout behavior remains unchanged.

4. **RTD task priority/frequency corrected**
   - Replaced hardcoded priority `20` with `RTD_PRIO`.
   - Confirmed loop delay uses `RTD_FREQ`.

5. **Brake light ownership conflict removed from BPPC**
   - `bppc_task` no longer directly drives `Brake_Light_GPIO` with a different threshold than `bse_task`.
   - `bse_task` remains the owner of brake-light output through `set_brakelight()`.

6. **PWM driver hardened**
   - Added null guards for `pwm_device_init` and `pwm_set_percent`.
   - Fixed duty computation to use floating-point scaling before converting to CCR integer.
   - Avoids dereferencing null CCR pointers.

7. **Flow sensor driver hardened**
   - Added null/clock guards.
   - Zero-capture case now clears frequency/duty instead of leaving stale values.
   - Stores `high_count` explicitly.

8. **ADC helper hardened**
   - `stm32f767_adc_read` now guards null handles.
   - Replaced `HAL_MAX_DELAY` conversion wait with bounded 10 ms timeout.
   - Returns zero on failed start/poll instead of blocking indefinitely.
   - `stm32f767_adc_switch_channel` now rejects null handles.

9. **Safety gating cleanup**
   - Cascadia enable/on are initialized low during startup and only driven after `error_task` evaluates the current fault inputs.
   - Cascadia ON preserves the original 3 second delay after Cascadia enable.
   - AMS stale, CAN RX/TX faults, and BMS/IMD/BSPD fail inputs now block the motor-controller torque command in `apps_task`.
   - CAN RX and TX fault ownership is split before being aggregated into `canbus_fault`.
   - BSE range and dual-sensor plausibility checks are both active.
   - RTD entry now requires the safety-relevant ECU fault state to be clear, and RTD exits if one of those faults appears.

10. **Task creation cleanup**
    - Task handles are assigned before assertion checks, so release builds with `NDEBUG` cannot compile out task creation side effects.

11. **MISRA baseline Rev 2 cleanup**
    - Added null guards to first-party driver initialization and write/read helper paths.
    - Removed `strtok()` from CLI tokenization and kept bounded token-limit behavior.
    - Removed remaining first-party `strlen()` use from ISR-adjacent UART callback logic.
    - Converted `map()` away from `long double` arithmetic.
    - Added unsigned suffixes to first-party integer macros.
    - Made STM32 mutex attributes file-local `static const`.
    - Initialized RTC local structs before HAL use.

12. **MISRA logic/detail clean pass**
    - `tokenize()` now uses the caller-provided token capacity instead of silently reserving one unused slot.
    - CLI command dispatch now handles empty/whitespace-only commands without dereferencing `argv[0]`.
    - PWM initialization now rejects invalid channels and propagates `HAL_TIM_PWM_Start` failure.
    - Flow sensor initialization records timer start failures, and impossible high-count > total-count captures are clamped before duty calculation.
    - CAN transmit now rejects IDs outside the standard 11-bit range used by this driver.
    - MPU6050 config validation now covers all enum fields, and reads reject invalid zero scale divisors.
    - `REG_USR_CTRL` was renamed to `REG_PWR_MGMT_1` to match the actual register address being written.
    - `ntc_init()` now has a null guard.

## Items intentionally not changed

- Fault policy in `error_task.c` was not changed because hard/soft fault behavior may be a team/rules design choice.
- Coolant fault remains effectively disabled because calibration appears unfinished and forcing it active could create unwanted shutdown behavior. This still needs hardware calibration before vehicle use.
- RTD state machine semantics were not changed beyond priority/frequency cleanup.
- ISR-based AMS parsing was left intact because the parser is bounded and small; moving to a queue would be a bigger architectural change.
- CLI UART echo behavior was not rewritten because it changes interactive behavior and needs hardware validation.

## Validation run

From `host_tests/`:

```text
make CC=gcc clean
make CC=gcc ci
```

Result:

```text
ALL ECU UNIT TESTS PASSED
ALL ECU UNIT TESTS PASSED
ALL ECU DRIVER TESTS PASSED
ALL ECU HOST TESTS PASSED
GCC analyzer passed on host-testable ECU parser and driver code
```
