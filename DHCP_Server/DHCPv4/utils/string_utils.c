#define _GNU_SOURCE
#include <ctype.h>
#include <string.h>
#include "../include/string_utils.h"

char *trim(char *str)
{
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
    if (str[0] == '"')
        str++;
    int len = strlen(str);
    if (len > 0 && str[len - 1] == '"')
        str[len - 1] = '\0';
    return str;
}