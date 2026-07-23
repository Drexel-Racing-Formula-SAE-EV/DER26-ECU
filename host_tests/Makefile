CC ?= gcc
CLANG ?= clang
BUILD_DIR := build
ECU_ROOT := ..

CFLAGS_COMMON := -std=c11 -g -O0 -Wall -Wextra -Werror -Wno-unused-parameter \
	-DECU_HOST_TEST=1 \
	-I$(ECU_ROOT)/Core/Inc \
	-I$(ECU_ROOT)/Core/Inc/ext_drivers

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
POWER_GOLDEN_SRC := power/ams_v034_golden_vector_test.c $(ECU_ROOT)/Core/Src/ext_drivers/ams_power_consumer.c
POWER_GOLDEN_BIN := $(BUILD_DIR)/ams_v034_golden_vector_tests
LEGACY_AMS_GATE_FLAG := -DAMS_POWER_AUTHORITY_REQUIRED_FOR_TORQUE=0

.PHONY: help clean unit test power-consumer power-integration power-golden system-sil stress asan ubsan analyze clang-analyze profile-gates ci make-build-dir list

help:
	@echo "Targets:"
	@echo "  make unit       Build and run focused ECU unit tests"
	@echo "  make test       Build and run ECU host regression tests"
	@echo "  make power-consumer Run AMS power protocol v2 conformance tests"
	@echo "  make power-integration Run ECU AMS parser/torque-gate integration tests"
	@echo "  make power-golden Run AMS v0.3.4 producer-generated golden-vector test"
	@echo "  make system-sil Build and run ECU system SIL/fault-injection tests"
	@echo "  make stress     Run system SIL with extended deterministic fuzz cycles"
	@echo "  make asan       Run all host tests with AddressSanitizer + UBSan"
	@echo "  make ubsan      Run all host tests with UBSan only"
	@echo "  make analyze    Run GCC analyzer on host-testable ECU code"
	@echo "  make profile-gates Verify bench/vehicle build interlocks"
	@echo "  make ci         Run unit, regression, power protocol, SIL, gates, and analyzer targets"
	@echo "  make clean      Remove build outputs"

make-build-dir:
	mkdir -p $(BUILD_DIR)

unit: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(LEGACY_AMS_GATE_FLAG) $(UNIT_SRC) -o $(UNIT_BIN) $(LDLIBS)
	$(UNIT_BIN)

power-consumer: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(POWER_CONSUMER_SRC) -o $(POWER_CONSUMER_BIN) $(LDLIBS)
	$(POWER_CONSUMER_BIN)

power-integration: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(POWER_INTEGRATION_SRC) -o $(POWER_INTEGRATION_BIN) $(LDLIBS)
	$(POWER_INTEGRATION_BIN)

power-golden: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(POWER_GOLDEN_SRC) -o $(POWER_GOLDEN_BIN) $(LDLIBS)
	$(POWER_GOLDEN_BIN)

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

analyze: make-build-dir
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(POWER_CONSUMER_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(UNIT_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(SIL_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(POWER_INTEGRATION_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(POWER_GOLDEN_SRC)

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
		echo "ERROR: vehicle profile compiled without AMS power-clamp implementation"; rm -f $$err; exit 1; \
	elif ! grep -q "Vehicle profile requires the conservative AMS DCL/CCL torque clamp implementation" $$err; then \
		echo "ERROR: vehicle profile failed for an unexpected reason"; cat $$err; rm -f $$err; exit 1; \
	else \
		echo "PASS vehicle profile is locked without the numeric AMS power-clamp implementation"; rm -f $$err; \
	fi
	@err=$$(mktemp); \
	if $(CC) $(CFLAGS_COMMON) -DECU_BUILD_PROFILE=1 -DECU_BSPD_INTERFACE_3V3_VALIDATED=1 \
		-DECU_CM200_CAN_CONTRACT_VALIDATED=1 -DECU_AMS_POWER_CLAMP_VALIDATED=1 \
		-fsyntax-only $(CORE_SRC) >/dev/null 2>$$err; then \
		echo "ERROR: vehicle profile bypassed source-owned AMS clamp implementation lock"; rm -f $$err; exit 1; \
	elif ! grep -q "Vehicle profile requires the conservative AMS DCL/CCL torque clamp implementation" $$err; then \
		echo "ERROR: fully acknowledged vehicle profile failed for an unexpected reason"; cat $$err; rm -f $$err; exit 1; \
	else \
		echo "PASS external acknowledgements cannot bypass the source-owned AMS clamp implementation lock"; rm -f $$err; \
	fi

clang-analyze: make-build-dir
	$(CLANG) $(CFLAGS_COMMON) --analyze -Xanalyzer -analyzer-output=text $(POWER_CONSUMER_SRC)
	$(CLANG) $(CFLAGS_COMMON) --analyze -Xanalyzer -analyzer-output=text $(UNIT_SRC)
	$(CLANG) $(CFLAGS_COMMON) --analyze -Xanalyzer -analyzer-output=text $(SRC)
	$(CLANG) $(CFLAGS_COMMON) --analyze -Xanalyzer -analyzer-output=text $(SIL_SRC)
	$(CLANG) $(CFLAGS_COMMON) --analyze -Xanalyzer -analyzer-output=text $(POWER_INTEGRATION_SRC)
	$(CLANG) $(CFLAGS_COMMON) --analyze -Xanalyzer -analyzer-output=text $(POWER_GOLDEN_SRC)

ci: unit test power-consumer power-integration power-golden system-sil profile-gates analyze

list:
	@grep -R -E '^static void test_' src unit system | sed -E 's/^.*static void (test_[^(]+).*/\1/' | sort

clean:
	rm -rf $(BUILD_DIR)
