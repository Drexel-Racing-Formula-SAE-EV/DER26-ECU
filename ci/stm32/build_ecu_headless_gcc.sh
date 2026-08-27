#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(pwd)"
BUILD_DIR="$ROOT_DIR/build"
ECU_BUILD_PROFILE="${ECU_BUILD_PROFILE:-0}"
ECU_BSPD_INTERFACE_3V3_VALIDATED="${ECU_BSPD_INTERFACE_3V3_VALIDATED:-0}"
ECU_CM200_CAN_CONTRACT_VALIDATED="${ECU_CM200_CAN_CONTRACT_VALIDATED:-0}"
ECU_AMS_POWER_CLAMP_VALIDATED="${ECU_AMS_POWER_CLAMP_VALIDATED:-0}"

if [[ "$ECU_BUILD_PROFILE" != "0" && "$ECU_BUILD_PROFILE" != "1" ]]; then
  echo "ECU_BUILD_PROFILE must be 0 (bench) or 1 (vehicle)"
  exit 1
fi
for value_name in ECU_BSPD_INTERFACE_3V3_VALIDATED ECU_CM200_CAN_CONTRACT_VALIDATED ECU_AMS_POWER_CLAMP_VALIDATED; do
  value="${!value_name}"
  if [[ "$value" != "0" && "$value" != "1" ]]; then
    echo "$value_name must be 0 or 1"
    exit 1
  fi
done

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

CFLAGS=(
  -mcpu=cortex-m7
  -mthumb
  -mfpu=fpv5-d16
  -mfloat-abi=hard
  -std=gnu11
  -O0
  -g3
  -ffunction-sections
  -fdata-sections
  -Wall
  -Wextra
  -Wno-unused-parameter
  -Wno-missing-field-initializers
  -Werror=implicit-function-declaration
  -Werror=int-conversion
  -DDEBUG
  -DUSE_HAL_DRIVER
  -DSTM32F767xx
  "-DECU_BUILD_PROFILE=${ECU_BUILD_PROFILE}"
  "-DECU_BSPD_INTERFACE_3V3_VALIDATED=${ECU_BSPD_INTERFACE_3V3_VALIDATED}"
  "-DECU_CM200_CAN_CONTRACT_VALIDATED=${ECU_CM200_CAN_CONTRACT_VALIDATED}"
  "-DECU_AMS_POWER_CLAMP_VALIDATED=${ECU_AMS_POWER_CLAMP_VALIDATED}"
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
  -I"$ROOT_DIR/FATFS/App"
  -I"$ROOT_DIR/FATFS/Target"
  -I"$ROOT_DIR/Middlewares/Third_Party/FatFs/src"
)

SOURCES=()
while IFS= read -r src; do SOURCES+=("$src"); done < <(find "$ROOT_DIR/Core/Src" -name "*.c" -print | sort)
while IFS= read -r src; do SOURCES+=("$src"); done < <(find "$ROOT_DIR/Drivers/STM32F7xx_HAL_Driver/Src" -name "*.c" -print | sort)
while IFS= read -r src; do SOURCES+=("$src"); done < <(find "$ROOT_DIR/FATFS" -name "*.c" -print | sort)
while IFS= read -r src; do SOURCES+=("$src"); done < <(find "$ROOT_DIR/Middlewares/Third_Party/FatFs/src" -name "*.c" -print | sort)

FREERTOS_SOURCES=(
  "$ROOT_DIR/Middlewares/Third_Party/FreeRTOS/Source/croutine.c"
  "$ROOT_DIR/Middlewares/Third_Party/FreeRTOS/Source/event_groups.c"
  "$ROOT_DIR/Middlewares/Third_Party/FreeRTOS/Source/list.c"
  "$ROOT_DIR/Middlewares/Third_Party/FreeRTOS/Source/queue.c"
  "$ROOT_DIR/Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c"
  "$ROOT_DIR/Middlewares/Third_Party/FreeRTOS/Source/tasks.c"
  "$ROOT_DIR/Middlewares/Third_Party/FreeRTOS/Source/timers.c"
  "$ROOT_DIR/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c"
  "$ROOT_DIR/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1/port.c"
)

FREERTOS_CONFIG="$ROOT_DIR/Core/Inc/FreeRTOSConfig.h"
if grep -Eq '^[[:space:]]*#define[[:space:]]+configSUPPORT_DYNAMIC_ALLOCATION[[:space:]]+1([[:space:]]|$)' \
    "$FREERTOS_CONFIG"; then
  FREERTOS_SOURCES+=(
    "$ROOT_DIR/Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c"
  )
elif grep -Eq '^[[:space:]]*#define[[:space:]]+configSUPPORT_DYNAMIC_ALLOCATION[[:space:]]+0([[:space:]]|$)' \
    "$FREERTOS_CONFIG"; then
  echo "FreeRTOS static-only build: heap_4.c excluded."
else
  echo "Unable to resolve configSUPPORT_DYNAMIC_ALLOCATION in $FREERTOS_CONFIG"
  exit 1
fi

for src in "${FREERTOS_SOURCES[@]}"; do
  if [[ -f "$src" ]]; then
    SOURCES+=("$src")
  else
    echo "Missing expected FreeRTOS source: $src"
    exit 1
  fi
done

OBJECTS=()
echo "Compiling ${#SOURCES[@]} C files (ECU profile ${ECU_BUILD_PROFILE})..."
for src in "${SOURCES[@]}"; do
  rel="${src#$ROOT_DIR/}"
  obj="$BUILD_DIR/${rel//\//_}.o"
  arm-none-eabi-gcc "${CFLAGS[@]}" "${INCLUDES[@]}" -c "$src" -o "$obj"
  OBJECTS+=("$obj")
done

STARTUP="$ROOT_DIR/Core/Startup/startup_stm32f767zitx.s"
STARTUP_OBJ="$BUILD_DIR/startup_stm32f767zitx.o"
arm-none-eabi-gcc "${CFLAGS[@]}" "${INCLUDES[@]}" -x assembler-with-cpp -c "$STARTUP" -o "$STARTUP_OBJ"
OBJECTS+=("$STARTUP_OBJ")

LDFLAGS=(
  -mcpu=cortex-m7
  -mthumb
  -mfpu=fpv5-d16
  -mfloat-abi=hard
  -T"$ROOT_DIR/STM32F767ZITX_FLASH.ld"
  -Wl,-Map="$BUILD_DIR/DER26-ECU.map"
  -Wl,--gc-sections
  -specs=nano.specs
  -specs=nosys.specs
  -lc
  -lm
  -lnosys
)

echo "Linking DER26-ECU.elf..."
arm-none-eabi-gcc "${OBJECTS[@]}" "${LDFLAGS[@]}" -o "$BUILD_DIR/DER26-ECU.elf"
arm-none-eabi-size "$BUILD_DIR/DER26-ECU.elf"
arm-none-eabi-objcopy -O ihex "$BUILD_DIR/DER26-ECU.elf" "$BUILD_DIR/DER26-ECU.hex"
arm-none-eabi-objcopy -O binary "$BUILD_DIR/DER26-ECU.elf" "$BUILD_DIR/DER26-ECU.bin"
arm-none-eabi-objdump -h -S "$BUILD_DIR/DER26-ECU.elf" > "$BUILD_DIR/DER26-ECU.list"

echo "Headless STM32 ARM-GCC build complete."
