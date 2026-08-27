#!/usr/bin/env bash
set -euo pipefail

required_paths=(
  ".cproject"
  "Core/Inc/app.h"
  "Core/Inc/board.h"
  "Core/Inc/ecu_config.h"
  "Core/Inc/ext_drivers/ams.h"
  "Core/Src/ext_drivers/ams.c"
  "Core/Inc/ext_drivers/ams_power_consumer.h"
  "Core/Src/ext_drivers/ams_power_consumer.c"
  "Core/Inc/ext_drivers/cm200.h"
  "Core/Src/ext_drivers/cm200.c"
  "Core/Inc/power/ecu_pack_current_model.h"
  "Core/Src/power/ecu_pack_current_model.c"
  "Core/Inc/power/ecu_torque_clamp.h"
  "Core/Src/power/ecu_torque_clamp.c"
  "Core/Inc/power/ecu_current_residual_monitor.h"
  "Core/Src/power/ecu_current_residual_monitor.c"
  "docs/ECU_TORQUE_TO_PACK_CURRENT_CLAMP.md"
  "docs/ECU_CURRENT_MODEL_CERTIFICATION_CAMPAIGN.md"
  "Core/Src/tasks/canbus_task.c"
  "Core/Src/stm32f7xx_it.c"
  "docs/BSPD_INTERFACE_AND_TEST_PLAN.md"
  "docs/ECU_HARDWARE_BRINGUP.md"
  "docs/CM200_CAN_CONTRACT_AND_BRINGUP.md"
  "docs/ECU_AMS_CAN_CONTRACT.md"
  "docs/ECU_AMS_POWER_PROTOCOL_V2.md"
  "docs/ECU_TORQUE_REMOVAL_AND_AVAILABILITY_BUDGET.md"
  "ci/scripts/check_residual_authority_gate.sh"
  "ci/scripts/check_current_model_evidence_gate.sh"
  "ci/scripts/check_core_host_syntax.sh"
  "Core/Startup/startup_stm32f767zitx.s"
  "Drivers/CMSIS"
  "Drivers/STM32F7xx_HAL_Driver"
  "Middlewares/Third_Party/FreeRTOS"
  "STM32F767ZITX_FLASH.ld"
  "host_tests/Makefile"
  "host_tests/power/ams_power_consumer_test.c"
  "host_tests/power/ams_power_integration_test.c"
  "host_tests/power/ams_v036_golden_vector_test.c"
  "host_tests/power/ams_source_compat_test.c"
  "host_tests/power/torque_clamp_test.c"
  "host_tests/power/rev7_invariant_probe.c"
  "host_tests/power/bundle_availability_probe.c"
  "host_tests/power/current_residual_monitor_test.c"
  "host_tests/power/current_model_vector_dump.c"
  "Tools/current_model/current_model_oracle.py"
  "Tools/current_model/differential_check.py"
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

# The ECU subscribes to bxCAN TX/RX0/SCE notification classes. Keep the NVIC,
# vector handlers, and CubeMX project in sync so bus-off/error handling cannot
# become dependent on unrelated RX traffic.
for irq in CAN1_TX CAN1_RX0 CAN1_SCE; do
  grep -q "HAL_NVIC_EnableIRQ(${irq}_IRQn)" Core/Src/stm32f7xx_hal_msp.c || { echo "Missing ${irq} NVIC enable"; exit 1; }
  grep -q "HAL_NVIC_DisableIRQ(${irq}_IRQn)" Core/Src/stm32f7xx_hal_msp.c || { echo "Missing ${irq} NVIC disable"; exit 1; }
  grep -q "void ${irq}_IRQHandler(void)" Core/Src/stm32f7xx_it.c || { echo "Missing ${irq} IRQ handler"; exit 1; }
done

grep -q 'NVIC.CAN1_TX_IRQn=true' DER26-ECU.ioc || { echo "CubeMX must enable CAN1_TX_IRQn"; exit 1; }
grep -q 'NVIC.CAN1_RX0_IRQn=true' DER26-ECU.ioc || { echo "CubeMX must enable CAN1_RX0_IRQn"; exit 1; }
grep -q 'NVIC.CAN1_SCE_IRQn=true' DER26-ECU.ioc || { echo "CubeMX must enable CAN1_SCE_IRQn"; exit 1; }

for notification in CAN_IT_TX_MAILBOX_EMPTY CAN_IT_RX_FIFO0_MSG_PENDING CAN_IT_RX_FIFO0_OVERRUN CAN_IT_ERROR_WARNING CAN_IT_ERROR_PASSIVE CAN_IT_BUSOFF CAN_IT_LAST_ERROR_CODE CAN_IT_ERROR; do
  grep -q "$notification" Core/Src/app.c || { echo "Missing CAN notification: $notification"; exit 1; }
done

# This package is an explicit BENCH Validation target. Propulsion outputs are
# source-inhibited by ECU_BUILD_PROFILE_BENCH while sensing, CAN, logging, CLI
# and coolant-pump manual control remain available for validation.
python3 -c 'import xml.etree.ElementTree as E; E.parse(".cproject")'
[[ "$(grep -c 'name="BENCH Validation Debug"' .cproject)" -ge 1 ]] || { echo "Missing BENCH Validation Debug CubeIDE configuration"; exit 1; }
[[ "$(grep -c 'name="BENCH Validation Release"' .cproject)" -ge 1 ]] || { echo "Missing BENCH Validation Release CubeIDE configuration"; exit 1; }
[[ "$(grep -c 'value="ECU_BUILD_PROFILE=0"' .cproject)" -eq 2 ]] || { echo "Both ECU CubeIDE configurations must select BENCH"; exit 1; }
[[ "$(grep -c 'value="DER26_CAN_BITRATE_KBPS=500"' .cproject)" -eq 2 ]] || { echo "Both ECU CubeIDE configurations must select 500 kbit/s"; exit 1; }

echo "Project structure check passed."
