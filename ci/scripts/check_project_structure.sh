#!/usr/bin/env bash
set -euo pipefail

required_paths=(
  "Core/Inc/app.h"
  "Core/Inc/board.h"
  "Core/Inc/ext_drivers/ams.h"
  "Core/Src/ext_drivers/ams.c"
  "Core/Src/tasks/canbus_task.c"
  "Core/Src/stm32f7xx_it.c"
  "Core/Startup/startup_stm32f767zitx.s"
  "Drivers/CMSIS"
  "Drivers/STM32F7xx_HAL_Driver"
  "Middlewares/Third_Party/FreeRTOS"
  "STM32F767ZITX_FLASH.ld"
  "host_tests/Makefile"
)

for path in "${required_paths[@]}"; do
  if [[ ! -e "$path" ]]; then
    echo "Missing required path: $path"
    exit 1
  fi
done

echo "Project structure check passed."
