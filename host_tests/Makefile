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

.PHONY: help clean unit test torque-clamp rev7-invariants bundle-availability residual-monitor elapsed-timer current-model-differential power-consumer power-integration power-golden power-source-compat system-sil stress asan ubsan analyze clang-analyze profile-gates static-allocation-gate residual-authority-gate current-model-evidence-gate ci clang-ci make-build-dir list

help:
	@echo "Targets:"
	@echo "  make unit       Build and run focused ECU unit tests"
	@echo "  make test       Build and run ECU host regression tests"
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
	@echo "  make stress     Run system SIL with extended deterministic fuzz cycles"
	@echo "  make asan       Run all host tests with AddressSanitizer + UBSan"
	@echo "  make ubsan      Run all host tests with UBSan only"
	@echo "  make analyze    Run GCC analyzer on host-testable ECU code"
	@echo "  make profile-gates Verify bench/vehicle build interlocks"
	@echo "  make static-allocation-gate Reject dynamic application RTOS objects"
	@echo "  make ci         Run unit, regression, power protocol, SIL, gates, and analyzer targets"
	@echo "  make clean      Remove build outputs"

make-build-dir:
	mkdir -p $(BUILD_DIR)

unit: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(LEGACY_AMS_GATE_FLAG) $(UNIT_SRC) -o $(UNIT_BIN) $(LDLIBS)
	$(UNIT_BIN)

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

stress: CFLAGS_COMMON += -DECU_HOST_LONG_FUZZ_CYCLES=50000u
stress: system-sil

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

analyze: make-build-dir
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(POWER_CONSUMER_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(BUNDLE_AVAILABILITY_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(UNIT_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(SIL_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(POWER_INTEGRATION_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(POWER_GOLDEN_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(ELAPSED_TIMER_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(REV7_INVARIANT_SRC)

profile-gates:
	$(CC) $(CFLAGS_COMMON) -DECU_BUILD_PROFILE=0 -fsyntax-only $(CORE_SRC)
	@err=$$(mktemp); \
	if $(CC) $(CFLAGS_COMMON) -DECU_BUILD_PROFILE=1 -fsyntax-only $(CORE_SRC) >/dev/null 2>$$err; then \
		echo "ERROR: unsafe vehicle profile compiled without BSPD interface acknowledgement"; rm -f $$err; exit 1; \
	elif ! grep -q "Vehicle profile requires a validated 12 V BSPD-OK" $$err; then \
		echo "ERROR: vehicle profile failed for an unexpected reason"; cat $$err; rm -f $$err; exit 1; \
	else \
		echo "PASS vehicle profile is locked without BSPD interface acknowledgement"; rm -f $$err; \
	fi
	@err=$$(mktemp); \
	if $(CC) $(CFLAGS_COMMON) -DECU_BUILD_PROFILE=1 -DECU_BSPD_INTERFACE_3V3_VALIDATED=1 -fsyntax-only $(CORE_SRC) >/dev/null 2>$$err; then \
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
		-DECU_SAFE_OUTPUTS_VALIDATED=1 -fsyntax-only $(CORE_SRC) >/dev/null 2>$$err; then \
		echo "PASS all independent vehicle release acknowledgements satisfy compile-time gates"; rm -f $$err; \
	else \
		echo "ERROR: fully acknowledged vehicle profile failed unexpectedly"; cat $$err; rm -f $$err; exit 1; \
	fi

clang-analyze: make-build-dir
	$(CLANG) $(CFLAGS_COMMON) --analyze -Xanalyzer -analyzer-output=text $(POWER_CONSUMER_SRC)
	$(CLANG) $(CFLAGS_COMMON) --analyze -Xanalyzer -analyzer-output=text $(BUNDLE_AVAILABILITY_SRC)
	$(CLANG) $(CFLAGS_COMMON) --analyze -Xanalyzer -analyzer-output=text $(UNIT_SRC)
	$(CLANG) $(CFLAGS_COMMON) --analyze -Xanalyzer -analyzer-output=text $(SRC)
	$(CLANG) $(CFLAGS_COMMON) --analyze -Xanalyzer -analyzer-output=text $(SIL_SRC)
	$(CLANG) $(CFLAGS_COMMON) --analyze -Xanalyzer -analyzer-output=text $(POWER_INTEGRATION_SRC)
	$(CLANG) $(CFLAGS_COMMON) --analyze -Xanalyzer -analyzer-output=text $(POWER_GOLDEN_SRC)
	$(CLANG) $(CFLAGS_COMMON) --analyze -Xanalyzer -analyzer-output=text $(TORQUE_CLAMP_SRC)
	$(CLANG) $(CFLAGS_COMMON) --analyze -Xanalyzer -analyzer-output=text $(RESIDUAL_MONITOR_SRC)
	$(CLANG) $(CFLAGS_COMMON) --analyze -Xanalyzer -analyzer-output=text $(ELAPSED_TIMER_SRC)
	$(CLANG) $(CFLAGS_COMMON) --analyze -Xanalyzer -analyzer-output=text $(REV7_INVARIANT_SRC)

residual-authority-gate:
	$(ECU_ROOT)/ci/scripts/check_residual_authority_gate.sh $(ECU_ROOT)

current-model-evidence-gate:
	$(ECU_ROOT)/ci/scripts/check_current_model_evidence_gate.sh $(ECU_ROOT)

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

ci: torque-clamp rev7-invariants bundle-availability residual-monitor elapsed-timer current-model-differential unit test power-consumer power-integration power-golden system-sil profile-gates static-allocation-gate residual-authority-gate current-model-evidence-gate analyze

clang-ci: torque-clamp rev7-invariants bundle-availability residual-monitor elapsed-timer current-model-differential unit test power-consumer power-integration power-golden system-sil profile-gates static-allocation-gate residual-authority-gate current-model-evidence-gate clang-analyze

list:
	@grep -R -E '^static void test_' src unit system | sed -E 's/^.*static void (test_[^(]+).*/\1/' | sort

clean:
	rm -rf $(BUILD_DIR)
