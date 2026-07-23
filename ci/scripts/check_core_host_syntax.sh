#!/usr/bin/env bash
set -euo pipefail

# This is a host parser/type-check pass for every ECU application source. It is
# not a replacement for the ARM build. Pointer-size warnings from CMSIS and the
# newlib task reentrancy object are disabled only for this x86 syntax check.

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

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

gcc "${COMMON[@]}" -DECU_BUILD_PROFILE=0 "${INCLUDES[@]}" "${SOURCES[@]}"

# Even with every external evidence acknowledgement present, the current
# vehicle build must remain locked until ecu_config.h's source-owned
# ECU_AMS_POWER_CLAMP_IMPLEMENTED latch is changed by the implementation commit.
vehicle_error="$(mktemp)"
trap 'rm -f "$vehicle_error"' EXIT
if gcc "${COMMON[@]}" -DECU_BUILD_PROFILE=1 -DECU_BSPD_INTERFACE_3V3_VALIDATED=1 \
  -DECU_CM200_CAN_CONTRACT_VALIDATED=1 -DECU_AMS_POWER_CLAMP_VALIDATED=1 \
  "${INCLUDES[@]}" "${SOURCES[@]}" >/dev/null 2>"$vehicle_error"; then
  echo "ERROR: vehicle profile bypassed the source-owned AMS power-clamp implementation lock"
  exit 1
fi
if ! grep -q "Vehicle profile requires the conservative AMS DCL/CCL torque clamp implementation" "$vehicle_error"; then
  echo "ERROR: vehicle profile failed for an unexpected reason"
  cat "$vehicle_error"
  exit 1
fi

echo "Full ECU application syntax check passed for bench profile; expected vehicle implementation lock verified."
