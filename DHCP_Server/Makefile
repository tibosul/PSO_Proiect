# =============================================================================
# DHCP Server Suite - Centralized Makefile
# =============================================================================

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -D_GNU_SOURCE -DHAVE_LOGGER
LDFLAGS = -pthread

# Directories
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin

# Include paths
INC_V4 = -IDHCPv4/include -Ilogger
INC_V6 = -IDHCPv6/include -Ilogger -IDHCPv4/include
INC_CLIENT = -IDHCPv4/include -IDHCPv6/include -Ilogger

# =============================================================================
# Source files
# =============================================================================

# Logger
LOGGER_SRC = logger/logger.c
LOGGER_OBJ = $(OBJ_DIR)/logger/logger.o

# DHCPv4 Server sources
V4_SRCS = DHCPv4/src/main.c \
          DHCPv4/src/config_v4.c \
          DHCPv4/src/ip_pool.c \
          DHCPv4/src/lease_v4.c \
          DHCPv4/src/dhcp_message.c \
          DHCPv4/utils/encoding_utils.c \
          DHCPv4/utils/file_utils.c \
          DHCPv4/utils/network_utils.c \
          DHCPv4/utils/string_utils.c \
          DHCPv4/utils/time_utils.c \
          DHCPv4/utils/thread_pool.c

V4_OBJS = $(OBJ_DIR)/v4/main.o \
          $(OBJ_DIR)/v4/config_v4.o \
          $(OBJ_DIR)/v4/ip_pool.o \
          $(OBJ_DIR)/v4/lease_v4.o \
          $(OBJ_DIR)/v4/dhcp_message.o \
          $(OBJ_DIR)/v4/encoding_utils.o \
          $(OBJ_DIR)/v4/file_utils.o \
          $(OBJ_DIR)/v4/network_utils.o \
          $(OBJ_DIR)/v4/string_utils.o \
          $(OBJ_DIR)/v4/time_utils.o \
          $(OBJ_DIR)/v4/thread_pool.o

# DHCPv6 Server sources
V6_SRCS = DHCPv6/sources/server.c \
          DHCPv6/sources/standalone.c \
          DHCPv6/sources/config_v6.c \
          DHCPv6/sources/leases6.c \
          DHCPv6/sources/utilsv6.c \
          DHCPv6/sources/ip6_pool.c \
          DHCPv6/sources/pd_pool.c \
          DHCPv6/sources/protocol_v6.c \
          DHCPv6/sources/config_v6_validate.c

V6_OBJS = $(OBJ_DIR)/v6/server.o \
          $(OBJ_DIR)/v6/standalone.o \
          $(OBJ_DIR)/v6/config_v6.o \
          $(OBJ_DIR)/v6/leases6.o \
          $(OBJ_DIR)/v6/utilsv6.o \
          $(OBJ_DIR)/v6/ip6_pool.o \
          $(OBJ_DIR)/v6/pd_pool.o \
          $(OBJ_DIR)/v6/protocol_v6.o \
          $(OBJ_DIR)/v6/config_v6_validate.o

# DHCPv6 Monitor
V6_MONITOR_OBJ = $(OBJ_DIR)/v6/monitor.o

# Client sources
CLIENT_V4_OBJ = $(OBJ_DIR)/client/client_v4.o
CLIENT_V6_OBJ = $(OBJ_DIR)/client/client_v6.o

# Shared objects (used by v6 client)
SHARED_TIME_OBJ = $(OBJ_DIR)/v4/time_utils.o
SHARED_PROTOCOL_OBJ = $(OBJ_DIR)/v6/protocol_v6.o
SHARED_UTILSV6_OBJ = $(OBJ_DIR)/v6/utilsv6.o

# =============================================================================
# Binaries
# =============================================================================

SERVER_V4 = $(BIN_DIR)/dhcpv4_server
SERVER_V6 = $(BIN_DIR)/dhcpv6_server
CLIENT_V4 = $(BIN_DIR)/dhcpv4_client
CLIENT_V6 = $(BIN_DIR)/dhcpv6_client
MONITOR_V6 = $(BIN_DIR)/dhcpv6_monitor

# =============================================================================
# Targets
# =============================================================================

.PHONY: all clean v4 v6 client servers clients help

all: servers clients
	@echo ""
	@echo "Build complete. Binaries in build/bin/"
	@ls -la $(BIN_DIR)/

servers: $(SERVER_V4) $(SERVER_V6) $(MONITOR_V6)

clients: $(CLIENT_V4) $(CLIENT_V6)

v4: $(SERVER_V4) $(CLIENT_V4)

v6: $(SERVER_V6) $(CLIENT_V6) $(MONITOR_V6)

client: clients

# =============================================================================
# Link binaries
# =============================================================================

$(SERVER_V4): $(V4_OBJS) $(LOGGER_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Built: $@"

$(SERVER_V6): $(V6_OBJS) $(LOGGER_OBJ) $(SHARED_TIME_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Built: $@"

$(CLIENT_V4): $(CLIENT_V4_OBJ) $(LOGGER_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "Built: $@"

$(CLIENT_V6): $(CLIENT_V6_OBJ) $(SHARED_PROTOCOL_OBJ) $(SHARED_UTILSV6_OBJ) $(LOGGER_OBJ) $(SHARED_TIME_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "Built: $@"

$(MONITOR_V6): $(V6_MONITOR_OBJ) $(LOGGER_OBJ) $(SHARED_TIME_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -lrt
	@echo "Built: $@"

# =============================================================================
# Compile object files
# =============================================================================

# Logger
$(LOGGER_OBJ): $(LOGGER_SRC)
	@mkdir -p $(OBJ_DIR)/logger
	$(CC) $(CFLAGS) -Ilogger -c $< -o $@

# DHCPv4 src/
$(OBJ_DIR)/v4/main.o: DHCPv4/src/main.c
	@mkdir -p $(OBJ_DIR)/v4
	$(CC) $(CFLAGS) $(INC_V4) -c $< -o $@

$(OBJ_DIR)/v4/config_v4.o: DHCPv4/src/config_v4.c
	@mkdir -p $(OBJ_DIR)/v4
	$(CC) $(CFLAGS) $(INC_V4) -c $< -o $@

$(OBJ_DIR)/v4/ip_pool.o: DHCPv4/src/ip_pool.c
	@mkdir -p $(OBJ_DIR)/v4
	$(CC) $(CFLAGS) $(INC_V4) -c $< -o $@

$(OBJ_DIR)/v4/lease_v4.o: DHCPv4/src/lease_v4.c
	@mkdir -p $(OBJ_DIR)/v4
	$(CC) $(CFLAGS) $(INC_V4) -c $< -o $@

$(OBJ_DIR)/v4/dhcp_message.o: DHCPv4/src/dhcp_message.c
	@mkdir -p $(OBJ_DIR)/v4
	$(CC) $(CFLAGS) $(INC_V4) -c $< -o $@

# DHCPv4 utils/
$(OBJ_DIR)/v4/encoding_utils.o: DHCPv4/utils/encoding_utils.c
	@mkdir -p $(OBJ_DIR)/v4
	$(CC) $(CFLAGS) $(INC_V4) -c $< -o $@

$(OBJ_DIR)/v4/file_utils.o: DHCPv4/utils/file_utils.c
	@mkdir -p $(OBJ_DIR)/v4
	$(CC) $(CFLAGS) $(INC_V4) -c $< -o $@

$(OBJ_DIR)/v4/network_utils.o: DHCPv4/utils/network_utils.c
	@mkdir -p $(OBJ_DIR)/v4
	$(CC) $(CFLAGS) $(INC_V4) -c $< -o $@

$(OBJ_DIR)/v4/string_utils.o: DHCPv4/utils/string_utils.c
	@mkdir -p $(OBJ_DIR)/v4
	$(CC) $(CFLAGS) $(INC_V4) -c $< -o $@

$(OBJ_DIR)/v4/time_utils.o: DHCPv4/utils/time_utils.c
	@mkdir -p $(OBJ_DIR)/v4
	$(CC) $(CFLAGS) $(INC_V4) -c $< -o $@

$(OBJ_DIR)/v4/thread_pool.o: DHCPv4/utils/thread_pool.c
	@mkdir -p $(OBJ_DIR)/v4
	$(CC) $(CFLAGS) $(INC_V4) -c $< -o $@

# DHCPv6 sources/
$(OBJ_DIR)/v6/server.o: DHCPv6/sources/server.c
	@mkdir -p $(OBJ_DIR)/v6
	$(CC) $(CFLAGS) $(INC_V6) -c $< -o $@

$(OBJ_DIR)/v6/standalone.o: DHCPv6/sources/standalone.c
	@mkdir -p $(OBJ_DIR)/v6
	$(CC) $(CFLAGS) $(INC_V6) -c $< -o $@

$(OBJ_DIR)/v6/config_v6.o: DHCPv6/sources/config_v6.c
	@mkdir -p $(OBJ_DIR)/v6
	$(CC) $(CFLAGS) $(INC_V6) -c $< -o $@

$(OBJ_DIR)/v6/leases6.o: DHCPv6/sources/leases6.c
	@mkdir -p $(OBJ_DIR)/v6
	$(CC) $(CFLAGS) $(INC_V6) -c $< -o $@

$(OBJ_DIR)/v6/utilsv6.o: DHCPv6/sources/utilsv6.c
	@mkdir -p $(OBJ_DIR)/v6
	$(CC) $(CFLAGS) $(INC_V6) -c $< -o $@

$(OBJ_DIR)/v6/ip6_pool.o: DHCPv6/sources/ip6_pool.c
	@mkdir -p $(OBJ_DIR)/v6
	$(CC) $(CFLAGS) $(INC_V6) -c $< -o $@

$(OBJ_DIR)/v6/pd_pool.o: DHCPv6/sources/pd_pool.c
	@mkdir -p $(OBJ_DIR)/v6
	$(CC) $(CFLAGS) $(INC_V6) -c $< -o $@

$(OBJ_DIR)/v6/protocol_v6.o: DHCPv6/sources/protocol_v6.c
	@mkdir -p $(OBJ_DIR)/v6
	$(CC) $(CFLAGS) $(INC_V6) -c $< -o $@

$(OBJ_DIR)/v6/config_v6_validate.o: DHCPv6/sources/config_v6_validate.c
	@mkdir -p $(OBJ_DIR)/v6
	$(CC) $(CFLAGS) $(INC_V6) -c $< -o $@

$(OBJ_DIR)/v6/monitor.o: DHCPv6/monitor/monitor.c
	@mkdir -p $(OBJ_DIR)/v6
	$(CC) $(CFLAGS) $(INC_V6) -c $< -o $@

# Clients
$(CLIENT_V4_OBJ): client/client_v4.c
	@mkdir -p $(OBJ_DIR)/client
	$(CC) $(CFLAGS) $(INC_CLIENT) -c $< -o $@

$(CLIENT_V6_OBJ): client/client_v6.c
	@mkdir -p $(OBJ_DIR)/client
	$(CC) $(CFLAGS) $(INC_CLIENT) -c $< -o $@

# =============================================================================
# Utility targets
# =============================================================================

clean:
	rm -rf $(BUILD_DIR)
	rm -f logs/*.log

logs:
	@echo "=== DHCPv4 Server Log ===" && tail -30 logs/dhcpv4.log 2>/dev/null || echo "(empty)"
	@echo ""
	@echo "=== DHCPv6 Server Log ===" && tail -30 logs/dhcpv6.log 2>/dev/null || echo "(empty)"

help:
	@echo "DHCP Server Suite - Makefile"
	@echo ""
	@echo "Build targets:"
	@echo "  make          - Build everything"
	@echo "  make servers  - Build all servers"
	@echo "  make clients  - Build all clients"
	@echo "  make v4       - Build DHCPv4 server + client"
	@echo "  make v6       - Build DHCPv6 server + client + monitor"
	@echo "  make clean    - Remove build directory"
	@echo ""
	@echo "Run targets:"
	@echo "  make run-v4   - Run DHCPv4 server (sudo)"
	@echo "  make run-v6   - Run DHCPv6 server (sudo)"
	@echo ""
	@echo "Binaries: build/bin/"

# =============================================================================
# Run targets
# =============================================================================

run-v4: $(SERVER_V4)
	@mkdir -p DHCPv4/data logs
	cd DHCPv4 && sudo ../$(SERVER_V4)

run-v6: $(SERVER_V6)
	@mkdir -p DHCPv6/leases logs
	cd DHCPv6 && sudo ../$(SERVER_V6)
