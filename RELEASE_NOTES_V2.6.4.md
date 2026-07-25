# DER26 ECU v2.6.4
## Static FreeRTOS / headless ARM-GCC build correction

### Problem

The v2.6.3 target build set `configSUPPORT_DYNAMIC_ALLOCATION` to `0`, but
still compiled two components that assume dynamic allocation is available:

- `heap_4.c`, which intentionally emits a compile-time error when dynamic
  allocation is disabled.
- The legacy STM32 CMSIS-RTOS2 adapter, whose dynamic fallback branches still
  referenced `xTaskCreate`, `xTimerCreate`, dynamic semaphore/queue creation,
  `pvPortMalloc`, and `vPortFree`.

The application tasks, queue, and mutex control blocks were already statically
allocated. The failure was in middleware/build integration rather than the ECU
application allocation model.

### Corrections

- Keep `configSUPPORT_DYNAMIC_ALLOCATION = 0`.
- Exclude `heap_4.c` from the headless ARM-GCC build when dynamic allocation is
  disabled.
- Exclude `heap_4.c` from STM32CubeIDE Debug and Release source entries.
- Compile the CMSIS-RTOS2 dynamic fallback paths only when
  `configSUPPORT_DYNAMIC_ALLOCATION == 1`.
- Preserve static CMSIS thread, mutex, semaphore, event-group, and queue paths.
- Reject CMSIS software-timer creation in the static-only configuration because
  this legacy adapter has no caller-owned field for its callback metadata and
  otherwise allocates that metadata with `pvPortMalloc`.
- Return zero from `osThreadEnumerate()` in the static-only adapter rather than
  allocating a temporary task-status array.
- Add target-build errors for implicit function declarations and integer/pointer
  conversion warnings so this class of middleware mismatch cannot silently
  compile.
- Make the headless build fail if it cannot resolve the dynamic-allocation
  setting from `FreeRTOSConfig.h`.

### Runtime impact

There is no change to torque-clamp logic, AMS authority handling, CM200 command
handling, task priorities, or safety policy.

All ECU application RTOS objects remain statically allocated. No heap
implementation is linked in the static-only target build.

### Validation

Completed in this environment:

- Full host `make ci`
- Revision 7 invariant probe
- AMS bundle-availability probe
- Current residual monitor tests
- ECU unit tests
- ECU system SIL tests
- Vehicle release-gate tests
- GCC analyzer targets
- Host preprocessing of the CMSIS adapter confirmed that no dynamic creation or
  allocation calls survive when dynamic allocation is disabled

The ARM GNU toolchain was unavailable in this container, so the final target
ELF/map was not generated locally. The GitHub headless ARM-GCC job is the final
confirmation for the target build.
