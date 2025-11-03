# DHCPv4 Server - Makefile
# Project structure with organized directories
# Automatic test detection and building

CC = gcc
CFLAGS = -Wall -Wextra -g -std=c11 -Iinclude
LDFLAGS =

# Directories
SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BUILD_DIR = build
CONFIG_DIR = config
DATA_DIR = data
UTILS_DIR = utils

# Automatically detect all source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

# Automatically detect all utils files
UTILS_SOURCES = $(wildcard $(UTILS_DIR)/*.c)
UTILS_OBJECTS = $(patsubst $(UTILS_DIR)/%.c,$(BUILD_DIR)/%.o,$(UTILS_SOURCES))

# All common objects that tests might need
COMMON_OBJECTS = $(OBJECTS) $(UTILS_OBJECTS)

# Automatically detect all headers
HEADERS = $(wildcard $(INC_DIR)/*.h)

# Automatically detect all test files
TEST_SOURCES = $(wildcard $(TEST_DIR)/test_*.c)
TEST_EXECUTABLES = $(patsubst $(TEST_DIR)/test_%.c,$(BUILD_DIR)/test_%,$(TEST_SOURCES))
TEST_NAMES = $(patsubst $(TEST_DIR)/test_%.c,%,$(TEST_SOURCES))

# All targets
.PHONY: all clean test help dirs list-tests run build

all: build

# Build target - only compiles, does not run tests
build: dirs $(TEST_EXECUTABLES)
	@echo ""
	@echo "=========================================="
	@echo "Build completed successfully!"
	@echo "=========================================="
	@echo "Built $(words $(TEST_EXECUTABLES)) test executable(s):"
	@for test in $(TEST_EXECUTABLES); do \
		echo "  - $$(basename $$test)"; \
	done
	@echo ""
	@echo "To run tests, use: make test"
	@echo "To run a specific test: make run TEST=<name>"
	@echo "=========================================="

# Create build directory
dirs:
	@mkdir -p $(BUILD_DIR)

# Object file compilation from src/
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Object file compilation from utils/
$(BUILD_DIR)/%.o: $(UTILS_DIR)/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Test object files
$(BUILD_DIR)/test_%.o: $(TEST_DIR)/test_%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Generic rule for linking any test executable
# Each test is linked with ALL common objects (all src + utils objects)
$(BUILD_DIR)/test_%: $(BUILD_DIR)/test_%.o $(COMMON_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Dynamic individual build targets for convenience
.SECONDEXPANSION:
$(TEST_NAMES): $$(BUILD_DIR)/test_$$@
	@echo "Built test: test_$@"

# Run all tests
test: dirs $(TEST_EXECUTABLES)
	@echo ""
	@echo "=========================================="
	@echo "Running all tests..."
	@echo "=========================================="
	@for test in $(TEST_EXECUTABLES); do \
		echo ""; \
		echo "=========================================="; \
		echo "Running: $$(basename $$test)"; \
		echo "=========================================="; \
		if [ "$$(basename $$test)" = "test_config_v4" ]; then \
			cd $(BUILD_DIR) && ./$$test ../$(CONFIG_DIR)/dhcpv4.conf || true; \
		elif [ "$$(basename $$test)" = "test_lease_extended" ]; then \
			cd $(BUILD_DIR) && ./$$test ../$(DATA_DIR)/dhcpv4.leases || true; \
		else \
			cd $(BUILD_DIR) && ./$$test || true; \
		fi; \
	done
	@echo ""
	@echo "=========================================="
	@echo "All tests completed!"
	@echo "=========================================="

# Run a specific test
# Usage: make run TEST=config_v4
# or: make run TEST=test_config_v4
run: dirs
	@if [ -z "$(TEST)" ]; then \
		echo "Error: Please specify TEST variable"; \
		echo "Usage: make run TEST=<test_name>"; \
		echo "Example: make run TEST=config_v4"; \
		echo "Available tests:"; \
		for test in $(TEST_NAMES); do echo "  - $$test"; done; \
		exit 1; \
	fi
	@TEST_NAME=$$(echo "$(TEST)" | sed 's/^test_//'); \
	if [ ! -f "$(TEST_DIR)/test_$$TEST_NAME.c" ]; then \
		echo "Error: Test 'test_$$TEST_NAME' not found"; \
		echo "Available tests:"; \
		for test in $(TEST_NAMES); do echo "  - $$test"; done; \
		exit 1; \
	fi
	@TEST_NAME=$$(echo "$(TEST)" | sed 's/^test_//'); \
	$(MAKE) $(BUILD_DIR)/test_$$TEST_NAME
	@echo "=========================================="
	@echo "Running: test_$$TEST_NAME"
	@echo "=========================================="
	@TEST_NAME=$$(echo "$(TEST)" | sed 's/^test_//'); \
	if [ "test_$$TEST_NAME" = "test_config_v4" ]; then \
		cd $(BUILD_DIR) && ./test_$$TEST_NAME ../$(CONFIG_DIR)/dhcpv4.conf; \
	elif [ "test_$$TEST_NAME" = "test_lease_extended" ]; then \
		cd $(BUILD_DIR) && ./test_$$TEST_NAME ../$(DATA_DIR)/dhcpv4.leases; \
	else \
		cd $(BUILD_DIR) && ./test_$$TEST_NAME; \
	fi

# List all available tests
list-tests:
	@echo "Available tests:"
	@for test in $(TEST_NAMES); do echo "  - $$test"; done

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	rm -f test_output.leases
	rm -f $(DATA_DIR)/test_output.leases
	rm -f *.tmp
	rm -f *~
	rm -f core
	@echo "Cleaned all build artifacts and temporary files"

# Help target
help:
	@echo "DHCPv4 Server Makefile"
	@echo "======================"
	@echo ""
	@echo "Project Structure:"
	@echo "  src/      - Source files (.c)"
	@echo "  include/  - Header files (.h)"
	@echo "  utils/    - Utility functions (string, network, time, encoding)"
	@echo "  tests/    - Test files (test_*.c)"
	@echo "  config/   - Configuration files"
	@echo "  data/     - Data files (leases)"
	@echo "  build/    - Build artifacts (generated)"
	@echo ""
	@echo "Features:"
	@echo "  - Automatic test detection (all test_*.c files)"
	@echo "  - Automatic dependency management"
	@echo "  - No need to modify Makefile when adding new tests"
	@echo ""
	@echo "Available targets:"
	@echo "  all                 - Build all test executables (default, no run)"
	@echo "  build               - Same as 'all' - only builds, does not run"
	@echo "  test                - Build and run all tests"
	@echo "  run TEST=<name>     - Build and run a specific test"
	@echo "  list-tests          - List all available tests"
	@echo "  clean               - Remove all generated files"
	@echo "  help                - Show this help message"
	@echo ""
	@echo "Dynamic test targets:"
	@echo "  Any test can be built individually by name (without 'test_' prefix)"
	@echo "  Example: make config_v4  (builds test_config_v4)"
	@echo ""
	@echo "Examples:"
	@echo "  make                       - Build all tests (no run)"
	@echo "  make build                 - Build all tests (same as make)"
	@echo "  make test                  - Build and run all tests"
	@echo "  make run TEST=config_v4    - Build and run test_config_v4"
	@echo "  make run TEST=ip_pool      - Build and run test_ip_pool"
	@echo "  make list-tests            - Show all available tests"
	@echo "  make clean                 - Clean everything"
	@echo ""
	@echo "Note: When you add a new test_*.c file in tests/, it will be"
	@echo "      automatically detected and compiled without any changes to"
	@echo "      this Makefile!"
