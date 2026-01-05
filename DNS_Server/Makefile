# DNS
CC      := gcc
CFLAGS  := -Wall

cache_testing: 
	${CC} ./sources/dns_cache.c ./tests/cache_testing.c -Iinclude -o cache_testing

test_string_utils:
	${CC} ./utils/string_utils.c ./tests/test_string_utils.c -Iinclude -o test_string_utils

clean:
	rm cache_testing
	rm test_string_utils