CC ?= gcc
CLANG ?= clang
BUILD_DIR := build
ECU_ROOT := ..

CFLAGS_COMMON := -std=c11 -g -O0 -Wall -Wextra -Werror -Wno-unused-parameter \
	-DECU_HOST_TEST=1 \
	-I$(ECU_ROOT)/Core/Inc \
	-I$(ECU_ROOT)/Core/Inc/ext_drivers \
	-I$(ECU_ROOT)/Core/Inc/power

LDLIBS := -lm
# Bound Clang path exploration so analyzer runtime is deterministic in CI.
CLANG_ANALYZE_FLAGS := --analyze -Xanalyzer -analyzer-output=text \
	-Xanalyzer -analyzer-config -Xanalyzer max-nodes=50000
HOST_STUB_CFLAGS := -iquote . -iquote include -I. -Iinclude
STORAGE_CFLAGS := $(HOST_STUB_CFLAGS) \
	-I$(ECU_ROOT)/FATFS/App \
	-I$(ECU_ROOT)/FATFS/Target \
	-I$(ECU_ROOT)/Middlewares/Third_Party/FatFs/src

CORE_SRC := \
	$(ECU_ROOT)/Core/Src/ext_drivers/ams.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/ams_power_consumer.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/cm200.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/ecu_safety.c

SRC := src/ecu_host_test_runner.c $(CORE_SRC)
BIN := $(BUILD_DIR)/ecu_host_tests
UNIT_SRC := unit/ecu_unit_test_runner.c $(CORE_SRC)
UNIT_BIN := $(BUILD_DIR)/ecu_unit_tests
SIL_SRC := system/ecu_system_sil_runner.c $(CORE_SRC)
SIL_BIN := $(BUILD_DIR)/ecu_system_sil_tests
PREVEHICLE_SIL_SRC := system/ecu_prevehicle_safety_sil.c $(CORE_SRC)
PREVEHICLE_SIL_BIN := $(BUILD_DIR)/ecu_prevehicle_safety_sil
POWER_CONSUMER_SRC := power/ams_power_consumer_test.c $(ECU_ROOT)/Core/Src/ext_drivers/ams_power_consumer.c
POWER_CONSUMER_BIN := $(BUILD_DIR)/ams_power_consumer_tests
POWER_INTEGRATION_SRC := power/ams_power_integration_test.c $(CORE_SRC)
POWER_INTEGRATION_BIN := $(BUILD_DIR)/ams_power_integration_tests
POWER_GOLDEN_SRC := power/ams_v036_golden_vector_test.c $(ECU_ROOT)/Core/Src/ext_drivers/ams_power_consumer.c
POWER_GOLDEN_BIN := $(BUILD_DIR)/ams_v036_golden_vector_tests
POWER_SOURCE_COMPAT_SRC := power/ams_source_compat_test.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/ams_power_consumer.c
POWER_SOURCE_COMPAT_BIN := $(BUILD_DIR)/ams_source_compat_tests

POWER_MODEL_SRC := \
	$(ECU_ROOT)/Core/Src/power/ecu_pack_current_model.c \
	$(ECU_ROOT)/Core/Src/power/ecu_torque_clamp.c
TORQUE_CLAMP_SRC := power/torque_clamp_test.c $(POWER_MODEL_SRC)
TORQUE_CLAMP_BIN := $(BUILD_DIR)/torque_clamp_tests
REV7_INVARIANT_SRC := power/rev7_invariant_probe.c $(POWER_MODEL_SRC)
REV7_INVARIANT_BIN := $(BUILD_DIR)/rev7_invariant_probe
BUNDLE_AVAILABILITY_SRC := power/bundle_availability_probe.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/ams_power_consumer.c
BUNDLE_AVAILABILITY_BIN := $(BUILD_DIR)/bundle_availability_probe
RESIDUAL_MONITOR_SRC := power/current_residual_monitor_test.c \
	$(ECU_ROOT)/Core/Src/power/ecu_current_residual_monitor.c
RESIDUAL_MONITOR_BIN := $(BUILD_DIR)/current_residual_monitor_tests
ELAPSED_TIMER_SRC := unit/elapsed_fault_timer_test.c
ELAPSED_TIMER_BIN := $(BUILD_DIR)/elapsed_fault_timer_tests

DRIVER_PROD_SRC := \
	$(ECU_ROOT)/Core/Src/ext_drivers/map.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/poten.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/pressure_sensor.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/pwm.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/flow_sensor.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/cooling_control.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/ntc.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/cli.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/dashboard.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/mpu6050.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/canbus.c
DRIVER_SRC := drivers/ecu_driver_test_runner.c support/fake_hal.c $(DRIVER_PROD_SRC)
DRIVER_BIN := $(BUILD_DIR)/ecu_driver_tests

BOARD_INTEGRATION_SRC := system/board_init_integration_test.c support/fake_hal.c \
	$(ECU_ROOT)/Core/Src/board.c $(DRIVER_PROD_SRC) $(CORE_SRC)
BOARD_INTEGRATION_BIN := $(BUILD_DIR)/board_init_integration_tests

SDCARD_SERVICE_SRC := storage/sdcard_service_test.c storage/fake_storage.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/sdcard_service.c
SDCARD_SERVICE_BIN := $(BUILD_DIR)/sdcard_service_tests

CALIBRATION_ARTIFACT_SRC := power/calibration_artifact_test.c \
	$(ECU_ROOT)/Core/Src/power/ecu_pack_current_calibration.c \
	$(ECU_ROOT)/Core/Src/power/ecu_pack_current_model.c
CALIBRATION_ARTIFACT_BIN := $(BUILD_DIR)/calibration_artifact_test

CROSS_STRESS_SRC := stress/ecu_cross_module_stress.c support/fake_hal.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/map.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/poten.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/pressure_sensor.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/pwm.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/flow_sensor.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/cli.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/canbus.c $(CORE_SRC)
CROSS_STRESS_BIN := $(BUILD_DIR)/ecu_cross_module_stress

CURRENT_MODEL_VECTOR_SRC := power/current_model_vector_dump.c \
	$(ECU_ROOT)/Core/Src/power/ecu_pack_current_model.c
CURRENT_MODEL_VECTOR_BIN := $(BUILD_DIR)/current_model_vector_dump
PYTHON ?= python3
AMS_ROOT ?=
AMS_COMPAT_CFLAGS := \
	-DSTM32F767xx -DUSE_HAL_DRIVER -DAMS_HOST_TEST=1 \
	-Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
	-I$(AMS_ROOT) \
	-I$(AMS_ROOT)/Core/Inc \
	-I$(AMS_ROOT)/Core/Inc/tasks \
	-I$(AMS_ROOT)/Core/Inc/ext_drivers \
	-I$(AMS_ROOT)/Drivers/CMSIS/Include \
	-I$(AMS_ROOT)/Drivers/CMSIS/Device/ST/STM32F7xx/Include \
	-I$(AMS_ROOT)/Drivers/STM32F7xx_HAL_Driver/Inc \
	-I$(AMS_ROOT)/Drivers/STM32F7xx_HAL_Driver/Inc/Legacy \
	-I$(AMS_ROOT)/Middlewares/Third_Party/FreeRTOS/Source/include \
	-I$(AMS_ROOT)/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 \
	-I$(AMS_ROOT)/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1
LEGACY_AMS_GATE_FLAG := -DAMS_POWER_AUTHORITY_REQUIRED_FOR_TORQUE=0

.PHONY: help clean unit test drivers board-integration prevehicle-sil prevehicle-ci sdcard-service calibration-artifact cross-stress stress-system stress stress-long torque-clamp rev7-invariants bundle-availability residual-monitor elapsed-timer current-model-differential power-consumer power-integration power-golden power-source-compat system-sil asan ubsan analyze clang-analyze profile-gates static-allocation-gate residual-authority-gate current-model-evidence-gate host-test-manifest-gate task-contract-gate fault-path-gate sdcard-contract-gate data-logger-contract-gate data-logger-build-gate can-v4-contract-gate can-clock-contract-gate ci clang-ci make-build-dir list

help:
	@echo "Targets:"
	@echo "  make unit       Build and run focused ECU unit tests"
	@echo "  make test       Build and run ECU host regression tests"
	@echo "  make drivers    Run analog, PWM, flow, CLI, UART, IMU, and CAN driver tests"
	@echo "  make board-integration Run board_init wiring/integration tests with fake HAL"
	@echo "  make sdcard-service Run deterministic FatFs/SD service fault-injection tests"
	@echo "  make calibration-artifact Verify checked-in current calibration stays non-authoritative"
	@echo "  make torque-clamp Build and run torque/current clamp contract tests"
	@echo "  make rev7-invariants Run independent Revision 7 invariant probe"
	@echo "  make bundle-availability Enforce dropped-bundle outage budgets"
	@echo "  make residual-monitor Build and run pack-current residual monitor tests"
	@echo "  make elapsed-timer Build and run exact plausibility timing boundary tests"
	@echo "  make current-model-differential Run independent Python/C model vectors"
	@echo "  make power-consumer Run AMS power protocol v2 conformance tests"
	@echo "  make power-integration Run ECU AMS parser/torque-gate integration tests"
	@echo "  make power-golden Run AMS v0.3.6 producer-generated golden-vector test"
	@echo "  make power-source-compat AMS_ROOT=/path/to/AMS Compile the live AMS producer against the ECU consumer"
	@echo "  make system-sil Build and run ECU system SIL/fault-injection tests"
	@echo "  make prevehicle-sil Run authority/logger/CAN negative-authority SIL"
	@echo "  make prevehicle-ci Run full CI + sanitizers + long stress for pre-vehicle review"
	@echo "  make cross-stress Run 200k cross-module deterministic stress cycles"
	@echo "  make stress     Run extended system SIL plus cross-module stress"
	@echo "  make stress-long Run 250k system SIL fuzz and 2M cross-module cycles"
	@echo "  make asan       Run all host tests with AddressSanitizer + UBSan"
	@echo "  make ubsan      Run all host tests with UBSan only"
	@echo "  make analyze    Run GCC analyzer on host-testable ECU code"
	@echo "  make profile-gates Verify bench/vehicle build interlocks"
	@echo "  make static-allocation-gate Reject dynamic application RTOS objects"
	@echo "  make host-test-manifest-gate Require every Core/Src file to declare coverage"
	@echo "  make task-contract-gate Verify all ten tasks are static and startup-gated"
	@echo "  make fault-path-gate Verify CPU faults force outputs safe before trapping"
	@echo "  make sdcard-contract-gate Verify SPI/FAT32/no-card static contracts"
	@echo "  make data-logger-contract-gate Verify logger ISR/filesystem/safety contracts"
	@echo "  make data-logger-build-gate Syntax-compile the production logger with host stubs"
	@echo "  make can-v4-contract-gate Verify DER26-CAN-V4 IDs/filter/commit/feedback contracts"
	@echo "  make can-clock-contract-gate Verify 500k/250k HSE-derived CAN timing"
	@echo "  make ci         Run unit, regression, power protocol, SIL, gates, and analyzer targets"
	@echo "  make clean      Remove build outputs"

make-build-dir:
	mkdir -p $(BUILD_DIR)

unit: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(LEGACY_AMS_GATE_FLAG) $(UNIT_SRC) -o $(UNIT_BIN) $(LDLIBS)
	$(UNIT_BIN)

drivers: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(HOST_STUB_CFLAGS) $(DRIVER_SRC) -o $(DRIVER_BIN) $(LDLIBS)
	$(DRIVER_BIN)

board-integration: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(HOST_STUB_CFLAGS) $(LEGACY_AMS_GATE_FLAG) \
		$(BOARD_INTEGRATION_SRC) -o $(BOARD_INTEGRATION_BIN) $(LDLIBS)
	$(BOARD_INTEGRATION_BIN)

sdcard-service: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(STORAGE_CFLAGS) $(SDCARD_SERVICE_SRC) \
		-o $(SDCARD_SERVICE_BIN) $(LDLIBS)
	$(SDCARD_SERVICE_BIN)

calibration-artifact: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(CALIBRATION_ARTIFACT_SRC) \
		-o $(CALIBRATION_ARTIFACT_BIN) $(LDLIBS)
	$(CALIBRATION_ARTIFACT_BIN)

torque-clamp: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(TORQUE_CLAMP_SRC) -o $(TORQUE_CLAMP_BIN) $(LDLIBS)
	$(TORQUE_CLAMP_BIN)

rev7-invariants: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(REV7_INVARIANT_SRC) -o $(REV7_INVARIANT_BIN) $(LDLIBS)
	$(REV7_INVARIANT_BIN)

bundle-availability: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(BUNDLE_AVAILABILITY_SRC) -o $(BUNDLE_AVAILABILITY_BIN) $(LDLIBS)
	$(BUNDLE_AVAILABILITY_BIN)

residual-monitor: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(RESIDUAL_MONITOR_SRC) -o $(RESIDUAL_MONITOR_BIN) $(LDLIBS)
	$(RESIDUAL_MONITOR_BIN)

elapsed-timer: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(ELAPSED_TIMER_SRC) -o $(ELAPSED_TIMER_BIN) $(LDLIBS)
	$(ELAPSED_TIMER_BIN)

current-model-differential: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(CURRENT_MODEL_VECTOR_SRC) -o $(CURRENT_MODEL_VECTOR_BIN) $(LDLIBS)
	$(PYTHON) $(ECU_ROOT)/Tools/current_model/differential_check.py $(CURRENT_MODEL_VECTOR_BIN)

power-consumer: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(POWER_CONSUMER_SRC) -o $(POWER_CONSUMER_BIN) $(LDLIBS)
	$(POWER_CONSUMER_BIN)

power-integration: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(POWER_INTEGRATION_SRC) -o $(POWER_INTEGRATION_BIN) $(LDLIBS)
	$(POWER_INTEGRATION_BIN)

power-golden: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(POWER_GOLDEN_SRC) -o $(POWER_GOLDEN_BIN) $(LDLIBS)
	$(POWER_GOLDEN_BIN)


power-source-compat: make-build-dir
	@test -n "$(AMS_ROOT)" || \
		(echo "ERROR: set AMS_ROOT to the AMS firmware directory"; exit 1)
	@test -f "$(AMS_ROOT)/Core/Src/sop/ams_power_can.c" || \
		(echo "ERROR: AMS_ROOT does not contain Core/Src/sop/ams_power_can.c"; exit 1)
	$(CC) $(AMS_COMPAT_CFLAGS) $(CFLAGS_COMMON) \
		$(POWER_SOURCE_COMPAT_SRC) \
		$(AMS_ROOT)/Core/Src/sop/ams_power_can.c \
		-o $(POWER_SOURCE_COMPAT_BIN) $(LDLIBS)
	$(POWER_SOURCE_COMPAT_BIN)

test: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(LEGACY_AMS_GATE_FLAG) $(SRC) -o $(BIN) $(LDLIBS)
	$(BIN)

system-sil: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(LEGACY_AMS_GATE_FLAG) $(SIL_SRC) -o $(SIL_BIN) $(LDLIBS)
	$(SIL_BIN)

prevehicle-sil: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(PREVEHICLE_SIL_SRC) -o $(PREVEHICLE_SIL_BIN) $(LDLIBS)
	$(PREVEHICLE_SIL_BIN)

cross-stress: make-build-dir
	$(CC) $(CFLAGS_COMMON) -O2 $(HOST_STUB_CFLAGS) $(LEGACY_AMS_GATE_FLAG) \
		$(CROSS_STRESS_SRC) -o $(CROSS_STRESS_BIN) $(LDLIBS)
	$(CROSS_STRESS_BIN)

stress-system: CFLAGS_COMMON += -DECU_HOST_LONG_FUZZ_CYCLES=50000u
stress-system: system-sil

stress: stress-system cross-stress

stress-long: make-build-dir
	$(CC) $(CFLAGS_COMMON) -O2 -DECU_HOST_LONG_FUZZ_CYCLES=250000u \
		$(LEGACY_AMS_GATE_FLAG) $(SIL_SRC) -o $(SIL_BIN)-long $(LDLIBS)
	$(SIL_BIN)-long
	$(CC) $(CFLAGS_COMMON) -O2 -DECU_CROSS_STRESS_CYCLES=2000000u \
		$(HOST_STUB_CFLAGS) $(LEGACY_AMS_GATE_FLAG) $(CROSS_STRESS_SRC) \
		-o $(CROSS_STRESS_BIN)-long $(LDLIBS)
	$(CROSS_STRESS_BIN)-long

asan: make-build-dir
	$(CC) $(CFLAGS_COMMON) -fsanitize=address,undefined -fno-omit-frame-pointer $(POWER_CONSUMER_SRC) -o $(POWER_CONSUMER_BIN)-asan $(LDLIBS)
	ASAN_OPTIONS=detect_leaks=0 $(POWER_CONSUMER_BIN)-asan
	$(CC) $(CFLAGS_COMMON) $(LEGACY_AMS_GATE_FLAG) -fsanitize=address,undefined -fno-omit-frame-pointer $(UNIT_SRC) -o $(UNIT_BIN)-asan $(LDLIBS)
	ASAN_OPTIONS=detect_leaks=0 $(UNIT_BIN)-asan
	$(CC) $(CFLAGS_COMMON) $(LEGACY_AMS_GATE_FLAG) -fsanitize=address,undefined -fno-omit-frame-pointer $(SRC) -o $(BIN)-asan $(LDLIBS)
	ASAN_OPTIONS=detect_leaks=0 $(BIN)-asan
	$(CC) $(CFLAGS_COMMON) $(LEGACY_AMS_GATE_FLAG) -fsanitize=address,undefined -fno-omit-frame-pointer $(SIL_SRC) -o $(SIL_BIN)-asan $(LDLIBS)
	ASAN_OPTIONS=detect_leaks=0 $(SIL_BIN)-asan
	$(CC) $(CFLAGS_COMMON) -fsanitize=address,undefined -fno-omit-frame-pointer $(POWER_INTEGRATION_SRC) -o $(POWER_INTEGRATION_BIN)-asan $(LDLIBS)
	ASAN_OPTIONS=detect_leaks=0 $(POWER_INTEGRATION_BIN)-asan
	$(CC) $(CFLAGS_COMMON) -fsanitize=address,undefined -fno-omit-frame-pointer $(POWER_GOLDEN_SRC) -o $(POWER_GOLDEN_BIN)-asan $(LDLIBS)
	ASAN_OPTIONS=detect_leaks=0 $(POWER_GOLDEN_BIN)-asan
	$(CC) $(CFLAGS_COMMON) -fsanitize=address,undefined -fno-omit-frame-pointer $(TORQUE_CLAMP_SRC) -o $(TORQUE_CLAMP_BIN)-asan $(LDLIBS)
	ASAN_OPTIONS=detect_leaks=0 $(TORQUE_CLAMP_BIN)-asan
	$(CC) $(CFLAGS_COMMON) -fsanitize=address,undefined -fno-omit-frame-pointer $(REV7_INVARIANT_SRC) -o $(REV7_INVARIANT_BIN)-asan $(LDLIBS)
	ASAN_OPTIONS=detect_leaks=0 $(REV7_INVARIANT_BIN)-asan
	$(CC) $(CFLAGS_COMMON) -fsanitize=address,undefined -fno-omit-frame-pointer $(BUNDLE_AVAILABILITY_SRC) -o $(BUNDLE_AVAILABILITY_BIN)-asan $(LDLIBS)
	ASAN_OPTIONS=detect_leaks=0 $(BUNDLE_AVAILABILITY_BIN)-asan
	$(CC) $(CFLAGS_COMMON) -fsanitize=address,undefined -fno-omit-frame-pointer $(RESIDUAL_MONITOR_SRC) -o $(RESIDUAL_MONITOR_BIN)-asan $(LDLIBS)
	ASAN_OPTIONS=detect_leaks=0 $(RESIDUAL_MONITOR_BIN)-asan
	$(CC) $(CFLAGS_COMMON) -fsanitize=address,undefined -fno-omit-frame-pointer $(ELAPSED_TIMER_SRC) -o $(ELAPSED_TIMER_BIN)-asan $(LDLIBS)
	ASAN_OPTIONS=detect_leaks=0 $(ELAPSED_TIMER_BIN)-asan

	$(CC) $(CFLAGS_COMMON) $(HOST_STUB_CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer $(DRIVER_SRC) -o $(DRIVER_BIN)-asan $(LDLIBS)
	ASAN_OPTIONS=detect_leaks=0 $(DRIVER_BIN)-asan
	$(CC) $(CFLAGS_COMMON) $(HOST_STUB_CFLAGS) $(LEGACY_AMS_GATE_FLAG) -fsanitize=address,undefined -fno-omit-frame-pointer $(BOARD_INTEGRATION_SRC) -o $(BOARD_INTEGRATION_BIN)-asan $(LDLIBS)
	ASAN_OPTIONS=detect_leaks=0 $(BOARD_INTEGRATION_BIN)-asan
	$(CC) $(CFLAGS_COMMON) $(STORAGE_CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer $(SDCARD_SERVICE_SRC) -o $(SDCARD_SERVICE_BIN)-asan $(LDLIBS)
	ASAN_OPTIONS=detect_leaks=0 $(SDCARD_SERVICE_BIN)-asan
	$(CC) $(CFLAGS_COMMON) -fsanitize=address,undefined -fno-omit-frame-pointer $(CALIBRATION_ARTIFACT_SRC) -o $(CALIBRATION_ARTIFACT_BIN)-asan $(LDLIBS)
	ASAN_OPTIONS=detect_leaks=0 $(CALIBRATION_ARTIFACT_BIN)-asan
	$(CC) $(CFLAGS_COMMON) -O1 -DECU_CROSS_STRESS_CYCLES=25000u $(HOST_STUB_CFLAGS) $(LEGACY_AMS_GATE_FLAG) -fsanitize=address,undefined -fno-omit-frame-pointer $(CROSS_STRESS_SRC) -o $(CROSS_STRESS_BIN)-asan $(LDLIBS)
	ASAN_OPTIONS=detect_leaks=0 $(CROSS_STRESS_BIN)-asan

ubsan: make-build-dir
	$(CC) $(CFLAGS_COMMON) -fsanitize=undefined -fno-omit-frame-pointer $(POWER_CONSUMER_SRC) -o $(POWER_CONSUMER_BIN)-ubsan $(LDLIBS)
	$(POWER_CONSUMER_BIN)-ubsan
	$(CC) $(CFLAGS_COMMON) $(LEGACY_AMS_GATE_FLAG) -fsanitize=undefined -fno-omit-frame-pointer $(UNIT_SRC) -o $(UNIT_BIN)-ubsan $(LDLIBS)
	$(UNIT_BIN)-ubsan
	$(CC) $(CFLAGS_COMMON) $(LEGACY_AMS_GATE_FLAG) -fsanitize=undefined -fno-omit-frame-pointer $(SRC) -o $(BIN)-ubsan $(LDLIBS)
	$(BIN)-ubsan
	$(CC) $(CFLAGS_COMMON) $(LEGACY_AMS_GATE_FLAG) -fsanitize=undefined -fno-omit-frame-pointer $(SIL_SRC) -o $(SIL_BIN)-ubsan $(LDLIBS)
	$(SIL_BIN)-ubsan
	$(CC) $(CFLAGS_COMMON) -fsanitize=undefined -fno-omit-frame-pointer $(POWER_INTEGRATION_SRC) -o $(POWER_INTEGRATION_BIN)-ubsan $(LDLIBS)
	$(POWER_INTEGRATION_BIN)-ubsan
	$(CC) $(CFLAGS_COMMON) -fsanitize=undefined -fno-omit-frame-pointer $(POWER_GOLDEN_SRC) -o $(POWER_GOLDEN_BIN)-ubsan $(LDLIBS)
	$(POWER_GOLDEN_BIN)-ubsan
	$(CC) $(CFLAGS_COMMON) -fsanitize=undefined -fno-omit-frame-pointer $(TORQUE_CLAMP_SRC) -o $(TORQUE_CLAMP_BIN)-ubsan $(LDLIBS)
	$(TORQUE_CLAMP_BIN)-ubsan
	$(CC) $(CFLAGS_COMMON) -fsanitize=undefined -fno-omit-frame-pointer $(REV7_INVARIANT_SRC) -o $(REV7_INVARIANT_BIN)-ubsan $(LDLIBS)
	$(REV7_INVARIANT_BIN)-ubsan
	$(CC) $(CFLAGS_COMMON) -fsanitize=undefined -fno-omit-frame-pointer $(BUNDLE_AVAILABILITY_SRC) -o $(BUNDLE_AVAILABILITY_BIN)-ubsan $(LDLIBS)
	$(BUNDLE_AVAILABILITY_BIN)-ubsan
	$(CC) $(CFLAGS_COMMON) -fsanitize=undefined -fno-omit-frame-pointer $(RESIDUAL_MONITOR_SRC) -o $(RESIDUAL_MONITOR_BIN)-ubsan $(LDLIBS)
	$(RESIDUAL_MONITOR_BIN)-ubsan
	$(CC) $(CFLAGS_COMMON) -fsanitize=undefined -fno-omit-frame-pointer $(ELAPSED_TIMER_SRC) -o $(ELAPSED_TIMER_BIN)-ubsan $(LDLIBS)
	$(ELAPSED_TIMER_BIN)-ubsan

	$(CC) $(CFLAGS_COMMON) $(HOST_STUB_CFLAGS) -fsanitize=undefined -fno-omit-frame-pointer $(DRIVER_SRC) -o $(DRIVER_BIN)-ubsan $(LDLIBS)
	$(DRIVER_BIN)-ubsan
	$(CC) $(CFLAGS_COMMON) $(HOST_STUB_CFLAGS) $(LEGACY_AMS_GATE_FLAG) -fsanitize=undefined -fno-omit-frame-pointer $(BOARD_INTEGRATION_SRC) -o $(BOARD_INTEGRATION_BIN)-ubsan $(LDLIBS)
	$(BOARD_INTEGRATION_BIN)-ubsan
	$(CC) $(CFLAGS_COMMON) $(STORAGE_CFLAGS) -fsanitize=undefined -fno-omit-frame-pointer $(SDCARD_SERVICE_SRC) -o $(SDCARD_SERVICE_BIN)-ubsan $(LDLIBS)
	$(SDCARD_SERVICE_BIN)-ubsan
	$(CC) $(CFLAGS_COMMON) -fsanitize=undefined -fno-omit-frame-pointer $(CALIBRATION_ARTIFACT_SRC) -o $(CALIBRATION_ARTIFACT_BIN)-ubsan $(LDLIBS)
	$(CALIBRATION_ARTIFACT_BIN)-ubsan
	$(CC) $(CFLAGS_COMMON) -O1 -DECU_CROSS_STRESS_CYCLES=25000u $(HOST_STUB_CFLAGS) $(LEGACY_AMS_GATE_FLAG) -fsanitize=undefined -fno-omit-frame-pointer $(CROSS_STRESS_SRC) -o $(CROSS_STRESS_BIN)-ubsan $(LDLIBS)
	$(CROSS_STRESS_BIN)-ubsan

analyze: make-build-dir
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(POWER_CONSUMER_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(BUNDLE_AVAILABILITY_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(UNIT_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(SIL_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(PREVEHICLE_SIL_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(POWER_INTEGRATION_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(POWER_GOLDEN_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(ELAPSED_TIMER_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(REV7_INVARIANT_SRC)
	$(CC) $(CFLAGS_COMMON) $(HOST_STUB_CFLAGS) -fanalyzer -fsyntax-only $(DRIVER_SRC)
	$(CC) $(CFLAGS_COMMON) $(HOST_STUB_CFLAGS) $(LEGACY_AMS_GATE_FLAG) -fanalyzer -fsyntax-only $(BOARD_INTEGRATION_SRC)
	$(CC) $(CFLAGS_COMMON) $(STORAGE_CFLAGS) -fanalyzer -fsyntax-only $(SDCARD_SERVICE_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(CALIBRATION_ARTIFACT_SRC)

profile-gates:
	$(CC) $(CFLAGS_COMMON) -DECU_BUILD_PROFILE=0 -fsyntax-only $(CORE_SRC)
	$(CC) $(CFLAGS_COMMON) -DECU_BUILD_PROFILE=2 -fsyntax-only $(CORE_SRC)
	@grep -Eq '^#define ECU_BSPD_INTERFACE_3V3_VALIDATED[[:space:]]+1' ../Core/Inc/ecu_config.h || \
		(echo "ERROR: corrected BSPD 3.3 V interface evidence is not recorded in this hardware-revision package"; exit 1)
	@echo "PASS corrected BSPD interface is recorded as validated for this package"
	@err=$$(mktemp); \
	if $(CC) $(CFLAGS_COMMON) -DECU_BUILD_PROFILE=1 -fsyntax-only $(CORE_SRC) >/dev/null 2>$$err; then \
		echo "ERROR: vehicle profile compiled without CM200 contract acknowledgement"; rm -f $$err; exit 1; \
	elif ! grep -q "Vehicle profile requires validated CM200" $$err; then \
		echo "ERROR: vehicle profile failed for an unexpected reason"; cat $$err; rm -f $$err; exit 1; \
	else \
		echo "PASS vehicle profile is locked without CM200 contract acknowledgement"; rm -f $$err; \
	fi
	@err=$$(mktemp); \
	if $(CC) $(CFLAGS_COMMON) -DECU_BUILD_PROFILE=1 -DECU_BSPD_INTERFACE_3V3_VALIDATED=1 \
		-DECU_CM200_CAN_CONTRACT_VALIDATED=1 -fsyntax-only $(CORE_SRC) >/dev/null 2>$$err; then \
		echo "ERROR: vehicle profile compiled without AMS power-clamp validation evidence"; rm -f $$err; exit 1; \
	elif ! grep -q "Vehicle profile requires validation evidence for the conservative AMS DCL/CCL torque clamp" $$err; then \
		echo "ERROR: vehicle profile failed for an unexpected reason"; cat $$err; rm -f $$err; exit 1; \
	else \
		echo "PASS vehicle profile is locked without AMS power-clamp validation evidence"; rm -f $$err; \
	fi
	@err=$$(mktemp); \
	if $(CC) $(CFLAGS_COMMON) -DECU_BUILD_PROFILE=1 -DECU_BSPD_INTERFACE_3V3_VALIDATED=1 \
		-DECU_CM200_CAN_CONTRACT_VALIDATED=1 -DECU_AMS_POWER_CLAMP_VALIDATED=1 \
		-fsyntax-only $(CORE_SRC) >/dev/null 2>$$err; then \
		echo "ERROR: vehicle profile compiled without complete subsystem release evidence"; rm -f $$err; exit 1; \
	elif ! grep -q "Vehicle profile requires complete ECU" $$err; then \
		echo "ERROR: vehicle profile failed for an unexpected reason"; cat $$err; rm -f $$err; exit 1; \
	else \
		echo "PASS vehicle profile is locked without complete subsystem evidence"; rm -f $$err; \
	fi
	@err=$$(mktemp); \
	if $(CC) $(CFLAGS_COMMON) -DECU_BUILD_PROFILE=1 -DECU_BSPD_INTERFACE_3V3_VALIDATED=1 \
		-DECU_CM200_CAN_CONTRACT_VALIDATED=1 -DECU_AMS_POWER_CLAMP_VALIDATED=1 \
		-DECU_PINMAP_VALIDATED=1 -DECU_APPS_CALIBRATION_VALIDATED=1 \
		-DECU_BSE_CALIBRATION_VALIDATED=1 -DECU_DISCRETE_INPUTS_VALIDATED=1 \
		-DECU_AMS_PROTOCOL_VALIDATED=1 -DECU_CURRENT_MODEL_VALIDATED=1 \
		-DECU_CURRENT_RESIDUAL_VALIDATED=1 -DECU_COOLING_VALIDATED=1 \
		-DECU_RTOS_MEMORY_VALIDATED=1 -DECU_WCET_VALIDATED=1 \
		-DECU_CAN_LOAD_VALIDATED=1 -DECU_WATCHDOG_VALIDATED=1 \
		-DECU_SAFE_OUTPUTS_VALIDATED=1 \
		-DECU_CURRENT_RESIDUAL_UNCERTAINTY_NEG_0P1A=10u \
		-DECU_CURRENT_RESIDUAL_UNCERTAINTY_POS_0P1A=10u \
		-fsyntax-only $(CORE_SRC) >/dev/null 2>$$err; then \
		echo "PASS fully acknowledged vehicle profile parses after cooling implementation"; rm -f $$err; \
	else \
		echo "ERROR: fully acknowledged vehicle profile failed"; cat $$err; rm -f $$err; exit 1; \
	fi

clang-analyze: make-build-dir
	$(CLANG) $(CFLAGS_COMMON) $(CLANG_ANALYZE_FLAGS) $(POWER_CONSUMER_SRC)
	$(CLANG) $(CFLAGS_COMMON) $(CLANG_ANALYZE_FLAGS) $(BUNDLE_AVAILABILITY_SRC)
	$(CLANG) $(CFLAGS_COMMON) $(CLANG_ANALYZE_FLAGS) $(UNIT_SRC)
	$(CLANG) $(CFLAGS_COMMON) $(CLANG_ANALYZE_FLAGS) $(SRC)
	$(CLANG) $(CFLAGS_COMMON) $(CLANG_ANALYZE_FLAGS) $(SIL_SRC)
	$(CLANG) $(CFLAGS_COMMON) $(CLANG_ANALYZE_FLAGS) $(PREVEHICLE_SIL_SRC)
	$(CLANG) $(CFLAGS_COMMON) $(CLANG_ANALYZE_FLAGS) $(POWER_INTEGRATION_SRC)
	$(CLANG) $(CFLAGS_COMMON) $(CLANG_ANALYZE_FLAGS) $(POWER_GOLDEN_SRC)
	$(CLANG) $(CFLAGS_COMMON) $(CLANG_ANALYZE_FLAGS) $(TORQUE_CLAMP_SRC)
	$(CLANG) $(CFLAGS_COMMON) $(CLANG_ANALYZE_FLAGS) $(RESIDUAL_MONITOR_SRC)
	$(CLANG) $(CFLAGS_COMMON) $(CLANG_ANALYZE_FLAGS) $(ELAPSED_TIMER_SRC)
	$(CLANG) $(CFLAGS_COMMON) $(CLANG_ANALYZE_FLAGS) $(REV7_INVARIANT_SRC)
	$(CLANG) $(CFLAGS_COMMON) $(HOST_STUB_CFLAGS) $(CLANG_ANALYZE_FLAGS) $(DRIVER_SRC)
	$(CLANG) $(CFLAGS_COMMON) $(HOST_STUB_CFLAGS) $(LEGACY_AMS_GATE_FLAG) $(CLANG_ANALYZE_FLAGS) $(BOARD_INTEGRATION_SRC)
	$(CLANG) $(CFLAGS_COMMON) $(STORAGE_CFLAGS) $(CLANG_ANALYZE_FLAGS) $(SDCARD_SERVICE_SRC)
	$(CLANG) $(CFLAGS_COMMON) $(CLANG_ANALYZE_FLAGS) $(CALIBRATION_ARTIFACT_SRC)

residual-authority-gate:
	bash $(ECU_ROOT)/ci/scripts/check_residual_authority_gate.sh $(ECU_ROOT)

current-model-evidence-gate:
	bash $(ECU_ROOT)/ci/scripts/check_current_model_evidence_gate.sh $(ECU_ROOT)

host-test-manifest-gate:
	$(PYTHON) $(ECU_ROOT)/ci/scripts/check_host_test_manifest.py $(ECU_ROOT)

task-contract-gate:
	$(PYTHON) $(ECU_ROOT)/ci/scripts/check_task_contract.py $(ECU_ROOT)

fault-path-gate:
	$(PYTHON) $(ECU_ROOT)/ci/scripts/check_fault_path_gate.py $(ECU_ROOT)

sdcard-contract-gate:
	$(PYTHON) $(ECU_ROOT)/ci/scripts/check_sdcard_contract.py $(ECU_ROOT)

data-logger-contract-gate:
	$(PYTHON) $(ECU_ROOT)/ci/scripts/check_data_logger_contract.py $(ECU_ROOT)

data-logger-build-gate:
	$(CC) $(CFLAGS_COMMON) $(LEGACY_AMS_GATE_FLAG) $(STORAGE_CFLAGS) \
		-include include/cmsis_os.h -fsyntax-only \
		$(ECU_ROOT)/Core/Src/ext_drivers/ecu_data_logger.c

can-v4-contract-gate:
	$(PYTHON) $(ECU_ROOT)/ci/scripts/check_can_v4_contract.py $(ECU_ROOT)

can-clock-contract-gate:
	$(PYTHON) $(ECU_ROOT)/ci/scripts/check_can_clock_contract.py $(ECU_ROOT)
	$(PYTHON) -m py_compile $(ECU_ROOT)/Tools/decode_can_log.py
	$(PYTHON) $(ECU_ROOT)/ci/scripts/test_can_log_decoder.py $(ECU_ROOT)

static-allocation-gate:
	@! grep -R -n -E 'xTaskCreate\(' $(ECU_ROOT)/Core/Src/tasks $(ECU_ROOT)/Core/Src/app.c || \
		(echo "ERROR: dynamic task creation found"; exit 1)
	@! grep -R -n -E 'xQueueCreate\(' $(ECU_ROOT)/Core/Src $(ECU_ROOT)/Core/Inc || \
		(echo "ERROR: dynamic queue creation found"; exit 1)
	@! grep -R -n -E 'osMutexNew\(NULL\)' $(ECU_ROOT)/Core/Src || \
		(echo "ERROR: mutex without static control-block attributes found"; exit 1)
	@test $$(grep -R -h -E 'xTaskCreateStatic\(' $(ECU_ROOT)/Core/Src/tasks/*.c | wc -l) -eq 10 || \
		(echo "ERROR: expected 10 statically created application tasks"; exit 1)
	@grep -q 'configSUPPORT_DYNAMIC_ALLOCATION[[:space:]]*0' $(ECU_ROOT)/Core/Inc/FreeRTOSConfig.h || \
		(echo "ERROR: FreeRTOS dynamic allocation remains enabled"; exit 1)
	@echo "PASS ECU application RTOS objects are statically allocated"

ci: torque-clamp rev7-invariants bundle-availability residual-monitor elapsed-timer current-model-differential calibration-artifact unit test drivers board-integration sdcard-service power-consumer power-integration power-golden system-sil prevehicle-sil cross-stress profile-gates static-allocation-gate host-test-manifest-gate task-contract-gate fault-path-gate sdcard-contract-gate data-logger-contract-gate data-logger-build-gate can-v4-contract-gate can-clock-contract-gate residual-authority-gate current-model-evidence-gate analyze

clang-ci: torque-clamp rev7-invariants bundle-availability residual-monitor elapsed-timer current-model-differential calibration-artifact unit test drivers board-integration sdcard-service power-consumer power-integration power-golden system-sil prevehicle-sil cross-stress profile-gates static-allocation-gate host-test-manifest-gate task-contract-gate fault-path-gate sdcard-contract-gate data-logger-contract-gate data-logger-build-gate can-v4-contract-gate can-clock-contract-gate residual-authority-gate current-model-evidence-gate clang-analyze

prevehicle-ci: ci asan ubsan stress-long
	@echo "PASS ECU pre-vehicle CI, sanitizers and long stress"

list:
	@grep -R -E '^static void test_' src unit system drivers storage power stress | sed -E 's/^.*static void (test_[^(]+).*/\1/' | sort

clean:
	rm -rf $(BUILD_DIR)
