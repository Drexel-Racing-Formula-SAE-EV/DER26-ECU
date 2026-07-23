#!/usr/bin/env bash
set -euo pipefail

required_paths=(
  "Core/Inc/app.h"
  "Core/Inc/board.h"
  "Core/Inc/ecu_config.h"
  "Core/Inc/ext_drivers/ams.h"
  "Core/Src/ext_drivers/ams.c"
  "Core/Inc/ext_drivers/ams_power_consumer.h"
  "Core/Src/ext_drivers/ams_power_consumer.c"
  "Core/Inc/ext_drivers/cm200.h"
  "Core/Src/ext_drivers/cm200.c"
  "Core/Src/tasks/canbus_task.c"
  "Core/Src/stm32f7xx_it.c"
  "Core/docs/BSPD_INTERFACE_AND_TEST_PLAN.md"
  "Core/docs/ECU_HARDWARE_BRINGUP.md"
  "Core/docs/CM200_CAN_CONTRACT_AND_BRINGUP.md"
  "Core/docs/ECU_AMS_CAN_CONTRACT.md"
  "Core/docs/ECU_AMS_POWER_PROTOCOL_V2.md"
  "ci/scripts/check_core_host_syntax.sh"
  "Core/Startup/startup_stm32f767zitx.s"
  "Drivers/CMSIS"
  "Drivers/STM32F7xx_HAL_Driver"
  "Middlewares/Third_Party/FreeRTOS"
  "STM32F767ZITX_FLASH.ld"
  "host_tests/Makefile"
  "host_tests/power/ams_power_consumer_test.c"
  "host_tests/power/ams_power_integration_test.c"
  "host_tests/power/ams_v034_golden_vector_test.c"
)

for path in "${required_paths[@]}"; do
  if [[ ! -e "$path" ]]; then
    echo "Missing required path: $path"
    exit 1
  fi
done

# CubeMX regeneration must not silently reintroduce an all-pass CAN filter or
# pre-start notification activation in main.c.  The driver owns acceptance
# filters/startup; app_create() owns the single checked notification activation.
if grep -q "HAL_CAN_ConfigFilter" Core/Src/main.c; then
  echo "main.c must not configure CAN filters; canbus_device_init() is the sole owner"
  exit 1
fi
if grep -q "HAL_CAN_ActivateNotification" Core/Src/main.c; then
  echo "main.c must not activate CAN notifications before driver initialization"
  exit 1
fi
notification_count=$(grep -R --include='*.c' -c "HAL_CAN_ActivateNotification" Core/Src | awk -F: '{sum += $2} END {print sum + 0}')
if [[ "$notification_count" -ne 1 ]]; then
  echo "Expected exactly one checked CAN notification activation, found $notification_count"
  exit 1
fi
if ! grep -q "HAL_CAN_ActivateNotification(app.board.canbus.hcan" Core/Src/app.c; then
  echo "app_create() must own the checked post-start CAN notification activation"
  exit 1
fi

echo "Project structure check passed."
