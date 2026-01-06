# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -Iinclude -I../logger -g -pthread -DHAVE_LOGGER
LDFLAGS = -pthread

# Directories
SRC_DIR = src
UTILS_DIR = utils
TESTS_DIR = tests
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin
INCLUDE_DIR = include
LOGGER_DIR = ../logger

# Main Server
SERVER_SRC = $(SRC_DIR)/main.c
SERVER_BIN = $(BIN_DIR)/dhcpd

# Source files
CONFIG_SRCS = $(SRC_DIR)/config_v4.c
IP_POOL_SRCS = $(SRC_DIR)/ip_pool.c
LEASE_SRCS = $(SRC_DIR)/lease_v4.c
MESSAGE_SRCS = $(SRC_DIR)/dhcp_message.c

UTILS_SRCS = $(UTILS_DIR)/encoding_utils.c \
	         $(UTILS_DIR)/file_utils.c \
	         $(UTILS_DIR)/network_utils.c \
	         $(UTILS_DIR)/string_utils.c \
	         $(UTILS_DIR)/time_utils.c \
	         $(UTILS_DIR)/thread_pool.c

# Logger source
LOGGER_SRC = $(LOGGER_DIR)/logger.c

# Test sources
TEST_CONFIG_SRC = $(TESTS_DIR)/test_config.c
TEST_TIMER_SRC = $(TESTS_DIR)/test_timer.c
TEST_FULL_SRC = $(TESTS_DIR)/test_full_server.c

# Object files (in build/obj/)
CONFIG_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/src/%.o,$(CONFIG_SRCS))
IP_POOL_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/src/%.o,$(IP_POOL_SRCS))
LEASE_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/src/%.o,$(LEASE_SRCS))
MESSAGE_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/src/%.o,$(MESSAGE_SRCS))
UTILS_OBJS = $(patsubst $(UTILS_DIR)/%.c,$(OBJ_DIR)/utils/%.o,$(UTILS_SRCS))
LOGGER_OBJ = $(OBJ_DIR)/logger/logger.o

# Test executables (in build/bin/)
TEST_CONFIG = $(BIN_DIR)/test_config
TEST_TIMER = $(BIN_DIR)/test_timer
TEST_FULL = $(BIN_DIR)/test_full_server

# Default target
all: $(TEST_CONFIG) $(TEST_TIMER) $(TEST_FULL) $(SERVER_BIN)

# Main Server Executable
$(SERVER_BIN): $(SERVER_SRC) $(CONFIG_OBJS) $(IP_POOL_OBJS) $(LEASE_OBJS) $(MESSAGE_OBJS) $(UTILS_OBJS) $(LOGGER_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Create build directories
$(OBJ_DIR)/src $(OBJ_DIR)/utils $(OBJ_DIR)/logger $(BIN_DIR):
	mkdir -p $@

# Test config executable
$(TEST_CONFIG): $(TEST_CONFIG_SRC) $(CONFIG_OBJS) $(UTILS_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Test timer executable
$(TEST_TIMER): $(TEST_TIMER_SRC) $(LEASE_OBJS) $(UTILS_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Test full server executable
$(TEST_FULL): $(TEST_FULL_SRC) $(LEASE_OBJS) $(UTILS_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile source files from src/
$(OBJ_DIR)/src/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)/src
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile source files from utils/
$(OBJ_DIR)/utils/%.o: $(UTILS_DIR)/%.c | $(OBJ_DIR)/utils
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile logger
$(LOGGER_OBJ): $(LOGGER_SRC) | $(OBJ_DIR)/logger
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Rebuild everything
rebuild: clean all

# Run tests
run: $(TEST_CONFIG)
	$(TEST_CONFIG)

run-timer: $(TEST_TIMER)
	$(TEST_TIMER)

run-full: $(TEST_FULL)
	$(TEST_FULL)

# Phony targets
.PHONY: all clean rebuild run run-timer run-full server
