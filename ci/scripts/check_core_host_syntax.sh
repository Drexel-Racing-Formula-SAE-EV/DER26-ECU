#!/usr/bin/env bash
set -euo pipefail

# Host parser/type-check pass for every ECU application source. This is not a
# replacement for the ARM target build. Set CC=clang to use Clang locally; CI
# may keep the default GCC compiler.

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
CC_BIN="${CC:-gcc}"

COMMON=(
  -fsyntax-only
  -std=gnu11
  -Wall
  -Wextra
  -Werror
  -Wno-unused-parameter
  -Wno-missing-field-initializers
  -Wno-pointer-to-int-cast
  -Wno-int-to-pointer-cast
  -DconfigUSE_NEWLIB_REENTRANT=0
  -DDEBUG
  -DUSE_HAL_DRIVER
  -DSTM32F767xx
)

INCLUDES=(
  -I"$ROOT_DIR/Core/Inc"
  -I"$ROOT_DIR/Core/Inc/ext_drivers"
  -I"$ROOT_DIR/Core/Inc/tasks"
  -I"$ROOT_DIR/Drivers/STM32F7xx_HAL_Driver/Inc"
  -I"$ROOT_DIR/Drivers/STM32F7xx_HAL_Driver/Inc/Legacy"
  -I"$ROOT_DIR/Drivers/CMSIS/Device/ST/STM32F7xx/Include"
  -I"$ROOT_DIR/Drivers/CMSIS/Include"
  -I"$ROOT_DIR/Middlewares/Third_Party/FreeRTOS/Source/include"
  -I"$ROOT_DIR/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2"
  -I"$ROOT_DIR/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1"
)

SOURCES=()
while IFS= read -r src; do SOURCES+=("$src"); done < <(find "$ROOT_DIR/Core/Src" -name "*.c" -print | sort)

"$CC_BIN" "${COMMON[@]}" -DECU_BUILD_PROFILE=0 "${INCLUDES[@]}" "${SOURCES[@]}"

# The implementation latch is now source-owned and enabled. The default
# vehicle profile must still fail because external BSPD, CM200, and clamp
# validation evidence is absent.
vehicle_error="$(mktemp)"
trap 'rm -f "$vehicle_error"' EXIT
if "$CC_BIN" "${COMMON[@]}" -DECU_BUILD_PROFILE=1 \
  "${INCLUDES[@]}" "${SOURCES[@]}" >/dev/null 2>"$vehicle_error"; then
  echo "ERROR: default vehicle profile compiled without external evidence"
  exit 1
fi
if ! grep -q "Vehicle profile requires a validated 12 V BSPD-OK" "$vehicle_error"; then
  echo "ERROR: default vehicle profile failed for an unexpected reason"
  cat "$vehicle_error"
  exit 1
fi

# A fully acknowledged profile must parse now that the implementation exists.
"$CC_BIN" "${COMMON[@]}" -DECU_BUILD_PROFILE=1 \
  -DECU_BSPD_INTERFACE_3V3_VALIDATED=1 \
  -DECU_CM200_CAN_CONTRACT_VALIDATED=1 \
  -DECU_AMS_POWER_CLAMP_VALIDATED=1 \
  -DECU_PINMAP_VALIDATED=1 \
  -DECU_APPS_CALIBRATION_VALIDATED=1 \
  -DECU_BSE_CALIBRATION_VALIDATED=1 \
  -DECU_DISCRETE_INPUTS_VALIDATED=1 \
  -DECU_AMS_PROTOCOL_VALIDATED=1 \
  -DECU_CURRENT_MODEL_VALIDATED=1 \
  -DECU_CURRENT_RESIDUAL_VALIDATED=1 \
  -DECU_COOLING_VALIDATED=1 \
  -DECU_RTOS_MEMORY_VALIDATED=1 \
  -DECU_WCET_VALIDATED=1 \
  -DECU_CAN_LOAD_VALIDATED=1 \
  -DECU_WATCHDOG_VALIDATED=1 \
  -DECU_SAFE_OUTPUTS_VALIDATED=1 \
  "${INCLUDES[@]}" "${SOURCES[@]}"

echo "Full ECU application syntax check passed for bench and fully acknowledged vehicle profiles (${#SOURCES[@]} files); default vehicle evidence locks remain active."
