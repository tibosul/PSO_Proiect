# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -Iinclude -g
LDFLAGS =

# Directories
SRC_DIR = src
UTILS_DIR = utils
TESTS_DIR = tests
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin
INCLUDE_DIR = include

# Source files
CONFIG_SRCS = $(SRC_DIR)/config_v4.c
IP_POOL_SRCS = $(SRC_DIR)/ip_pool.c
LEASE_SRCS = $(SRC_DIR)/lease_v4.c

UTILS_SRCS = $(UTILS_DIR)/encoding_utils.c \
             $(UTILS_DIR)/file_utils.c \
             $(UTILS_DIR)/network_utils.c \
             $(UTILS_DIR)/string_utils.c \
             $(UTILS_DIR)/time_utils.c

# Test sources
TEST_CONFIG_SRC = $(TESTS_DIR)/test_config.c

# Object files (in build/obj/)
CONFIG_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/src/%.o,$(CONFIG_SRCS))
IP_POOL_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/src/%.o,$(IP_POOL_SRCS))
LEASE_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/src/%.o,$(LEASE_SRCS))
UTILS_OBJS = $(patsubst $(UTILS_DIR)/%.c,$(OBJ_DIR)/utils/%.o,$(UTILS_SRCS))

# Test executables (in build/bin/)
TEST_CONFIG = $(BIN_DIR)/test_config

# Default target
all: $(TEST_CONFIG)

# Create build directories
$(OBJ_DIR)/src $(OBJ_DIR)/utils $(BIN_DIR):
	mkdir -p $@

# Test config executable
$(TEST_CONFIG): $(TEST_CONFIG_SRC) $(CONFIG_OBJS) $(UTILS_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile source files from src/
$(OBJ_DIR)/src/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)/src
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile source files from utils/
$(OBJ_DIR)/utils/%.o: $(UTILS_DIR)/%.c | $(OBJ_DIR)/utils
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Rebuild everything
rebuild: clean all

# Run test
run: $(TEST_CONFIG)
	$(TEST_CONFIG)

# Phony targets
.PHONY: all clean rebuild run
