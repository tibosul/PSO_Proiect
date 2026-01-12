CC      := gcc
CFLAGS  := -Wall -Wextra -g
INCLUDES := -Iinclude

# Directoare
SRC_DIR := ./sources
UTILS_DIR := ./utils
TEST_DIR := ./tests

# Lista surselor pentru serverul principal
SERVER_SRCS := $(SRC_DIR)/dns_server.c \
               $(SRC_DIR)/dns_config.c \
               $(SRC_DIR)/zone_manager.c \
               $(SRC_DIR)/dns_cache.c \
               $(UTILS_DIR)/network_utils.c \
               $(SRC_DIR)/dns_parser.c \
               $(UTILS_DIR)/string_utils.c

# Numele executabilului final
TARGET := dns_server

all: $(TARGET)

# Compilarea serverului DNS
$(TARGET): $(SERVER_SRCS)
	$(CC) $(CFLAGS) $(SERVER_SRCS) $(INCLUDES) -o $(TARGET)
	@echo "Build successful! Run with: ./$(TARGET)"

# Teste 

cache_testing: 
	$(CC) $(CFLAGS) $(SRC_DIR)/dns_cache.c $(TEST_DIR)/cache_testing.c $(INCLUDES) -o cache_testing

test_string_utils:
	$(CC) $(CFLAGS) $(UTILS_DIR)/string_utils.c $(TEST_DIR)/test_string_utils.c $(INCLUDES) -o test_string_utils

# Curatare

clean:
	rm -f $(TARGET) cache_testing test_string_utils
	@echo "Cleaned up executables."