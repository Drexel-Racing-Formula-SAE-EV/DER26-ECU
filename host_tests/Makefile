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
	$(ECU_ROOT)/Core/Src/ext_drivers/cm200.c \
	$(ECU_ROOT)/Core/Src/ext_drivers/ecu_safety.c

SRC := src/ecu_host_test_runner.c $(CORE_SRC)
BIN := $(BUILD_DIR)/ecu_host_tests
UNIT_SRC := unit/ecu_unit_test_runner.c $(CORE_SRC)
UNIT_BIN := $(BUILD_DIR)/ecu_unit_tests
SIL_SRC := system/ecu_system_sil_runner.c $(CORE_SRC)
SIL_BIN := $(BUILD_DIR)/ecu_system_sil_tests

.PHONY: help clean unit test system-sil stress asan ubsan analyze clang-analyze profile-gates ci make-build-dir list

help:
	@echo "Targets:"
	@echo "  make unit       Build and run focused ECU unit tests"
	@echo "  make test       Build and run ECU host regression tests"
	@echo "  make system-sil Build and run ECU system SIL/fault-injection tests"
	@echo "  make stress     Run system SIL with extended deterministic fuzz cycles"
	@echo "  make asan       Run all host tests with AddressSanitizer + UBSan"
	@echo "  make ubsan      Run all host tests with UBSan only"
	@echo "  make analyze    Run GCC analyzer on host-testable ECU code"
	@echo "  make profile-gates Verify bench/vehicle build interlocks"
	@echo "  make ci         Run unit, regression, system SIL, and analyzer targets"
	@echo "  make clean      Remove build outputs"

make-build-dir:
	mkdir -p $(BUILD_DIR)

unit: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(UNIT_SRC) -o $(UNIT_BIN) $(LDLIBS)
	$(UNIT_BIN)

test: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(SRC) -o $(BIN) $(LDLIBS)
	$(BIN)

system-sil: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(SIL_SRC) -o $(SIL_BIN) $(LDLIBS)
	$(SIL_BIN)

stress: CFLAGS_COMMON += -DECU_HOST_LONG_FUZZ_CYCLES=50000u
stress: system-sil

asan: make-build-dir
	$(CC) $(CFLAGS_COMMON) -fsanitize=address,undefined -fno-omit-frame-pointer $(UNIT_SRC) -o $(UNIT_BIN)-asan $(LDLIBS)
	ASAN_OPTIONS=detect_leaks=0 $(UNIT_BIN)-asan
	$(CC) $(CFLAGS_COMMON) -fsanitize=address,undefined -fno-omit-frame-pointer $(SRC) -o $(BIN)-asan $(LDLIBS)
	ASAN_OPTIONS=detect_leaks=0 $(BIN)-asan
	$(CC) $(CFLAGS_COMMON) -fsanitize=address,undefined -fno-omit-frame-pointer $(SIL_SRC) -o $(SIL_BIN)-asan $(LDLIBS)
	ASAN_OPTIONS=detect_leaks=0 $(SIL_BIN)-asan

ubsan: make-build-dir
	$(CC) $(CFLAGS_COMMON) -fsanitize=undefined -fno-omit-frame-pointer $(UNIT_SRC) -o $(UNIT_BIN)-ubsan $(LDLIBS)
	$(UNIT_BIN)-ubsan
	$(CC) $(CFLAGS_COMMON) -fsanitize=undefined -fno-omit-frame-pointer $(SRC) -o $(BIN)-ubsan $(LDLIBS)
	$(BIN)-ubsan
	$(CC) $(CFLAGS_COMMON) -fsanitize=undefined -fno-omit-frame-pointer $(SIL_SRC) -o $(SIL_BIN)-ubsan $(LDLIBS)
	$(SIL_BIN)-ubsan

analyze: make-build-dir
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(UNIT_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(SIL_SRC)

profile-gates:
	$(CC) $(CFLAGS_COMMON) -DECU_BUILD_PROFILE=0 -fsyntax-only $(CORE_SRC)
	@if $(CC) $(CFLAGS_COMMON) -DECU_BUILD_PROFILE=1 -fsyntax-only $(CORE_SRC) >/dev/null 2>&1; then \
		echo "ERROR: unsafe vehicle profile compiled without BSPD interface acknowledgement"; exit 1; \
	else \
		echo "PASS vehicle profile is locked without BSPD interface acknowledgement"; \
	fi
	@if $(CC) $(CFLAGS_COMMON) -DECU_BUILD_PROFILE=1 -DECU_BSPD_INTERFACE_3V3_VALIDATED=1 -fsyntax-only $(CORE_SRC) >/dev/null 2>&1; then \
		echo "ERROR: vehicle profile compiled without CM200 contract acknowledgement"; exit 1; \
	else \
		echo "PASS vehicle profile is locked without CM200 contract acknowledgement"; \
	fi
	$(CC) $(CFLAGS_COMMON) -DECU_BUILD_PROFILE=1 -DECU_BSPD_INTERFACE_3V3_VALIDATED=1 \
		-DECU_CM200_CAN_CONTRACT_VALIDATED=1 -fsyntax-only $(CORE_SRC)
	@echo "PASS fully acknowledged vehicle profile compiles"

clang-analyze: make-build-dir
	$(CLANG) $(CFLAGS_COMMON) --analyze $(UNIT_SRC)
	$(CLANG) $(CFLAGS_COMMON) --analyze $(SRC)
	$(CLANG) $(CFLAGS_COMMON) --analyze $(SIL_SRC)

ci: unit test system-sil profile-gates analyze

list:
	@grep -R -E '^static void test_' src unit system | sed -E 's/^.*static void (test_[^(]+).*/\1/' | sort

clean:
	rm -rf $(BUILD_DIR)
