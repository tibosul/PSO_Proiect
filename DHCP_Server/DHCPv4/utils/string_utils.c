#define _GNU_SOURCE
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include "../include/string_utils.h"

char *trim(char *str)
{
    if (!str)
        return NULL;

    while (isspace((unsigned char)*str))
        str++;
    if (*str == 0)
        return str;

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;
    end[1] = '\0';

    return str;
}

char *remove_quotes(char *str)
{
    if (!str)
        return NULL;

    if (str[0] == '"')
        str++;
    int len = strlen(str);
    if (len > 0 && str[len - 1] == '"')
        str[len - 1] = '\0';
    return str;
}

int parse_uint32(const char *str, uint32_t *value)
{
    if (!str || !value)
        return -1;

    char* endptr;
    errno = 0;

    unsigned long val = strtoul(str, &endptr, 10);

    if (errno != 0)
        return -2;

    if (endptr == str || *endptr != '\0')
        return -2;

    if (val > UINT32_MAX)
        return -3;

    *value = (uint32_t)val;
    return 0;
}