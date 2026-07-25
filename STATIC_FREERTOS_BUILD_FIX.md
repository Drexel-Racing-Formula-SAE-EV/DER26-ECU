# Static FreeRTOS build fix

## Root cause

`configSUPPORT_DYNAMIC_ALLOCATION` was disabled correctly, but the target build
still compiled `heap_4.c` and an unguarded legacy CMSIS-RTOS2 adapter. The heap
file therefore stopped compilation by design, while the adapter emitted implicit
function declarations for dynamic FreeRTOS creation APIs.

## Correct build contract

When `configSUPPORT_DYNAMIC_ALLOCATION == 0`:

1. No FreeRTOS heap implementation is compiled or linked.
2. Application tasks, queues, and mutexes use caller-owned static storage.
3. CMSIS-RTOS2 object creation succeeds only when the required static control
   block and stack/storage fields are supplied.
4. Dynamic fallback APIs are not compiled.
5. Unsupported CMSIS APIs that inherently require adapter-owned heap storage
   fail explicitly rather than allocating.

## Expected headless-build preamble

```text
FreeRTOS static-only build: heap_4.c excluded.
Compiling 73 C files (ECU profile 0)...
```

The exact source count may change as files are added; the important point is
that `heap_4.c` is absent in static-only mode and the build emits no implicit
dynamic-allocation declarations.
