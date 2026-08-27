#!/usr/bin/env bash
set -euo pipefail

# Whole-application GCC analyzer pass for the buildable bench profile. This
# host parse does not replace the STM32/ARM target build. Vehicle evidence
# locks are checked separately by check_core_host_syntax.sh/profile-gates.

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

COMMON=(
  -fsyntax-only
  -fanalyzer
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
  -I"$ROOT_DIR/Core/Inc/power"
  -I"$ROOT_DIR/FATFS/App"
  -I"$ROOT_DIR/FATFS/Target"
  -I"$ROOT_DIR/Middlewares/Third_Party/FatFs/src"
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

echo "GCC full-source analyzer passed for bench profile (${#SOURCES[@]} files)."
