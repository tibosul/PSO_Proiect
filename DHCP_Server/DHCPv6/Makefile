# ===== DHCPv6 test – leases + config (logger extern + time_utils din v4) =====

CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Wshadow -Wpedantic -O2 -g -D_GNU_SOURCE -DHAVE_LOGGER
CPPFLAGS:= -Iinclude -I../logger -I../DHCPv4/include
LDFLAGS :=
LDLIBS  :=

# directoare
SRCDIR      := sources
OBJDIR      := build
BINDIR      := bin
LOGGER_DIR  := ../logger
DHCPV4_SRC  := ../DHCPv4/utils

# surse DHCPv6
SRCS_V6 := \
  $(SRCDIR)/config_v6.c \
  $(SRCDIR)/leases6.c \
  $(SRCDIR)/utilsv6.c \
  $(SRCDIR)/ip6_pool.c \
  $(SRCDIR)/pd_pool.c \
  $(SRCDIR)/config_v6_validate.c \
  $(SRCDIR)/config_v6_validate.c

SRCS_SERVER := \
  $(SRCDIR)/server.c \
  $(SRCDIR)/standalone.c \
  $(SRCDIR)/config_v6.c \
  $(SRCDIR)/leases6.c \
  $(SRCDIR)/utilsv6.c \
  $(SRCDIR)/ip6_pool.c \
  $(SRCDIR)/pd_pool.c \
  $(SRCDIR)/protocol_v6.c \
  $(SRCDIR)/config_v6_validate.c

SRCS_CLIENT := client/client.c \
  $(SRCDIR)/protocol_v6.c \
  $(SRCDIR)/utilsv6.c

SRCS_MONITOR := monitor/monitor.c

# surse externe
SRCS_LOGGER := $(LOGGER_DIR)/logger.c
SRCS_V4     := $(DHCPV4_SRC)/time_utils.c

OBJS := $(SRCS_V6:%.c=$(OBJDIR)/%.o) \
        $(SRCS_LOGGER:%.c=$(OBJDIR)/%.o) \
        $(SRCS_V4:%.c=$(OBJDIR)/%.o)

OBJS_SERVER = $(SRCS_SERVER:%.c=build/%.o) \
              $(SRCS_LOGGER:%.c=build/%.o) \
              $(SRCS_V4:%.c=build/%.o)

OBJS_CLIENT = $(SRCS_CLIENT:%.c=build/%.o) \
              $(SRCS_LOGGER:%.c=build/%.o) \
              $(SRCS_V4:%.c=build/%.o)

OBJS_MONITOR = $(SRCS_MONITOR:%.c=build/%.o) \
               $(SRCS_LOGGER:%.c=build/%.o) \
               $(SRCS_V4:%.c=build/%.o)


SERVER_BIN = $(BINDIR)/dhcpv6_server
CLIENT_BIN = $(BINDIR)/dhcpv6_client
MONITOR_BIN = $(BINDIR)/dhcpv6_monitor

.PHONY: all server client monitor
all: $(SERVER_BIN) $(CLIENT_BIN) $(MONITOR_BIN)

server: $(SERVER_BIN)
client: $(CLIENT_BIN)
monitor: $(MONITOR_BIN)



$(SERVER_BIN): $(OBJS_SERVER) | $(BINDIR)
	$(CC) $(CFLAGS) $(OBJS_SERVER) -o $@ $(LDFLAGS) $(LDLIBS) -lpthread

$(CLIENT_BIN): $(OBJS_CLIENT) | $(BINDIR)
	$(CC) $(CFLAGS) $(OBJS_CLIENT) -o $@ $(LDFLAGS) $(LDLIBS)

$(MONITOR_BIN): $(OBJS_MONITOR) | $(BINDIR)
	$(CC) $(CFLAGS) $(OBJS_MONITOR) -o $@ $(LDFLAGS) $(LDLIBS) -lrt

# pattern rule
$(OBJDIR)/%.o: %.c | $(OBJDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BINDIR) $(OBJDIR):
	mkdir -p $@

.PHONY: run
run: $(SERVER_BIN)
	./$(SERVER_BIN)

.PHONY: clean
clean:
	rm -rf $(OBJDIR) $(BINDIR)

.PHONY: rebuild
rebuild: clean all