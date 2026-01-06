# DHCP Client Makefile - builds both DHCPv4 and DHCPv6 clients

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -g -D_GNU_SOURCE
CPPFLAGS = -I../DHCPv4/include -I../DHCPv6/include -I../logger

# Directories
BINDIR = bin
OBJDIR = build

# DHCPv4 Client
CLIENT_V4_SRC = client_v4.c
CLIENT_V4_BIN = $(BINDIR)/dhcpv4_client
CLIENT_V4_OBJ = $(OBJDIR)/client_v4.o

# DHCPv6 Client
CLIENT_V6_SRC = client_v6.c
CLIENT_V6_BIN = $(BINDIR)/dhcpv6_client
CLIENT_V6_OBJ = $(OBJDIR)/client_v6.o

# DHCPv6 dependencies
DHCPV6_SRCS = ../DHCPv6/sources/protocol_v6.c ../DHCPv6/sources/utilsv6.c
DHCPV6_OBJS = $(OBJDIR)/protocol_v6.o $(OBJDIR)/utilsv6.o

# Logger dependency
LOGGER_SRC = ../logger/logger.c
LOGGER_OBJ = $(OBJDIR)/logger.o

# Time utils from DHCPv4
TIME_UTILS_SRC = ../DHCPv4/utils/time_utils.c
TIME_UTILS_OBJ = $(OBJDIR)/time_utils.o

.PHONY: all v4 v6 clean

all: v4 v6

v4: $(CLIENT_V4_BIN)

v6: $(CLIENT_V6_BIN)

# DHCPv4 Client
$(CLIENT_V4_BIN): $(CLIENT_V4_OBJ) | $(BINDIR)
	$(CC) $(CFLAGS) $(CLIENT_V4_OBJ) -o $@

$(CLIENT_V4_OBJ): $(CLIENT_V4_SRC) client_v4.h | $(OBJDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# DHCPv6 Client
$(CLIENT_V6_BIN): $(CLIENT_V6_OBJ) $(DHCPV6_OBJS) $(LOGGER_OBJ) $(TIME_UTILS_OBJ) | $(BINDIR)
	$(CC) $(CFLAGS) $(CLIENT_V6_OBJ) $(DHCPV6_OBJS) $(LOGGER_OBJ) $(TIME_UTILS_OBJ) -o $@

$(CLIENT_V6_OBJ): $(CLIENT_V6_SRC) client_v6.h | $(OBJDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -DHAVE_LOGGER -c $< -o $@

# DHCPv6 dependencies
$(OBJDIR)/protocol_v6.o: ../DHCPv6/sources/protocol_v6.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -DHAVE_LOGGER -c $< -o $@

$(OBJDIR)/utilsv6.o: ../DHCPv6/sources/utilsv6.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -DHAVE_LOGGER -c $< -o $@

# Logger
$(LOGGER_OBJ): $(LOGGER_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Time utils
$(TIME_UTILS_OBJ): $(TIME_UTILS_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Create directories
$(BINDIR) $(OBJDIR):
	mkdir -p $@

clean:
	rm -rf $(BINDIR) $(OBJDIR)

.PHONY: rebuild
rebuild: clean all
