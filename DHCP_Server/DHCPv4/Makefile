# DHCPv4 Server - Makefile
# Project structure with organized directories

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

# Source files
SOURCES = $(SRC_DIR)/config_v4.c $(SRC_DIR)/lease_v4.c $(SRC_DIR)/ip_pool.c
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

# Utils files
UTILS_SOURCES = $(UTILS_DIR)/string_utils.c $(UTILS_DIR)/network_utils.c $(UTILS_DIR)/time_utils.c $(UTILS_DIR)/encoding_utils.c
UTILS_OBJECTS = $(patsubst $(UTILS_DIR)/%.c,$(BUILD_DIR)/%.o,$(UTILS_SOURCES))

# Headers
HEADERS = $(INC_DIR)/config_v4.h $(INC_DIR)/lease_v4.h $(INC_DIR)/ip_pool.h $(INC_DIR)/string_utils.h $(INC_DIR)/network_utils.h $(INC_DIR)/time_utils.h $(INC_DIR)/encoding_utils.h

# Test files
TEST_SOURCES = $(wildcard $(TEST_DIR)/test_*.c)
TEST_EXECUTABLES = $(patsubst $(TEST_DIR)/test_%.c,$(BUILD_DIR)/test_%,$(TEST_SOURCES))

# All targets
.PHONY: all clean test help dirs

all: dirs $(TEST_EXECUTABLES)

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

# Test executables
$(BUILD_DIR)/test_config_v4: $(BUILD_DIR)/test_config_v4.o $(BUILD_DIR)/config_v4.o $(UTILS_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/test_lease_extended: $(BUILD_DIR)/test_lease_extended.o $(BUILD_DIR)/lease_v4.o $(UTILS_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/test_lease_save: $(BUILD_DIR)/test_lease_save.o $(BUILD_DIR)/lease_v4.o $(UTILS_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/test_ip_pool: $(BUILD_DIR)/test_ip_pool.o $(BUILD_DIR)/ip_pool.o $(BUILD_DIR)/config_v4.o $(BUILD_DIR)/lease_v4.o $(UTILS_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Individual build targets for convenience
test_config_v4: $(BUILD_DIR)/test_config_v4
test_lease_extended: $(BUILD_DIR)/test_lease_extended
test_lease_save: $(BUILD_DIR)/test_lease_save
test_ip_pool: $(BUILD_DIR)/test_ip_pool

# Run all tests
test: all
	@echo "=========================================="
	@echo "Running Configuration Tests..."
	@echo "=========================================="
	cd $(BUILD_DIR) && ./test_config_v4 ../$(CONFIG_DIR)/dhcpv4.conf
	@echo ""
	@echo "=========================================="
	@echo "Running Lease Extended Tests..."
	@echo "=========================================="
	cd $(BUILD_DIR) && ./test_lease_extended ../$(DATA_DIR)/dhcpv4.leases
	@echo ""
	@echo "=========================================="
	@echo "Running Lease Save Tests..."
	@echo "=========================================="
	cd $(BUILD_DIR) && ./test_lease_save
	@echo ""
	@echo "=========================================="
	@echo "Running IP Pool Tests..."
	@echo "=========================================="
	cd $(BUILD_DIR) && ./test_ip_pool
	@echo ""
	@echo "=========================================="
	@echo "All tests completed!"
	@echo "=========================================="

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
	@echo "  tests/    - Test files"
	@echo "  config/   - Configuration files"
	@echo "  data/     - Data files (leases)"
	@echo "  build/    - Build artifacts (generated)"
	@echo ""
	@echo "Available targets:"
	@echo "  all                 - Build all test executables (default)"
	@echo "  test_config_v4      - Build configuration test"
	@echo "  test_lease_extended - Build extended lease test"
	@echo "  test_lease_save     - Build lease save test"
	@echo "  test_ip_pool        - Build IP pool test"
	@echo "  test                - Build and run all tests"
	@echo "  clean               - Remove all generated files"
	@echo "  help                - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make                - Build all tests"
	@echo "  make test_config_v4 - Build only config test"
	@echo "  make test           - Build and run all tests"
	@echo "  make clean          - Clean everything"
