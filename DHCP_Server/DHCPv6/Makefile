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
  $(SRCDIR)/config_v6_validate.c \
  tests/test_conf_leases.c

# surse externe
SRCS_LOGGER := $(LOGGER_DIR)/logger.c
SRCS_V4     := $(DHCPV4_SRC)/time_utils.c

OBJS := $(SRCS_V6:%.c=$(OBJDIR)/%.o) \
        $(SRCS_LOGGER:%.c=$(OBJDIR)/%.o) \
        $(SRCS_V4:%.c=$(OBJDIR)/%.o)

BIN  := $(BINDIR)/test_leases_v6

.PHONY: all
all: $(BIN)

$(BIN): $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS) -lpthread

# pattern rule
$(OBJDIR)/%.o: %.c | $(OBJDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BINDIR) $(OBJDIR):
	mkdir -p $@

.PHONY: run
run: $(BIN)
	./$(BIN)

.PHONY: clean
clean:
	rm -rf $(OBJDIR) $(BINDIR)

.PHONY: rebuild
rebuild: clean all
