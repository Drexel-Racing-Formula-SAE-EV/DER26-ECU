CC ?= gcc
BUILD_DIR := build
ECU_ROOT := ..

CFLAGS_COMMON := -std=c11 -g -O0 -Wall -Wextra -Werror -Wno-unused-parameter \
	-DECU_HOST_TEST=1 \
	-Iinclude \
	-I$(ECU_ROOT)/Core/Inc \
	-I$(ECU_ROOT)/Core/Inc/ext_drivers

LDLIBS := -lm

SRC := src/ecu_host_test_runner.c $(ECU_ROOT)/Core/Src/ext_drivers/ams.c
BIN := $(BUILD_DIR)/ecu_host_tests
UNIT_SRC := unit/ecu_unit_test_runner.c $(ECU_ROOT)/Core/Src/ext_drivers/ams.c
UNIT_BIN := $(BUILD_DIR)/ecu_unit_tests

.PHONY: help clean unit test analyze ci make-build-dir

help:
	@echo "Targets:"
	@echo "  make unit     Build and run focused ECU unit tests"
	@echo "  make test     Build and run ECU host regression tests"
	@echo "  make analyze  Run GCC analyzer on unit and regression host-testable ECU code"
	@echo "  make ci       Run test and analyze"
	@echo "  make clean    Remove build outputs"

make-build-dir:
	mkdir -p $(BUILD_DIR)

unit: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(UNIT_SRC) -o $(UNIT_BIN) $(LDLIBS)
	$(UNIT_BIN)

test: make-build-dir
	$(CC) $(CFLAGS_COMMON) $(SRC) -o $(BIN) $(LDLIBS)
	$(BIN)

analyze: make-build-dir
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(UNIT_SRC)
	$(CC) $(CFLAGS_COMMON) -fanalyzer -fsyntax-only $(SRC)

ci: unit test analyze

clean:
	rm -rf $(BUILD_DIR)
