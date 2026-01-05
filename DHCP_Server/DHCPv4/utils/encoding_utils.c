#define _GNU_SOURCE
#include <stdio.h>
#include <ctype.h>
#include "../include/utils/encoding_utils.h"

int parse_client_id_from_string(const char *str, uint8_t *client_id, uint32_t *len)
{
    if (!str || !client_id || !len)
        return -1;

    *len = 0;
    const char *ptr = str;

    // Skip leading whitespace and quotes
    while (*ptr && (isspace(*ptr) || *ptr == '"'))
        ptr++;

    // Parse octal escapes (\NNN) and regular characters
    while (*ptr && *ptr != '"' && *len < MAX_CLIENT_ID_LEN)
    {
        if (*ptr == '\\')
        {
            ptr++;
            if (*ptr >= '0' && *ptr <= '7')
            {
                // Octal escape sequence \NNN
                int value = 0;
                for (int i = 0; i < 3 && *ptr >= '0' && *ptr <= '7'; i++, ptr++)
                {
                    value = value * 8 + (*ptr - '0');
                }
                client_id[(*len)++] = (uint8_t)value;
            }
            else if (*ptr == 'x')
            {
                // Hex escape sequence \xNN
                ptr++;
                int value = 0;
                for (int i = 0; i < 2 && isxdigit(*ptr); i++, ptr++)
                {
                    value = value * 16 + (isdigit(*ptr) ? (*ptr - '0') : (tolower(*ptr) - 'a' + 10));
                }
                client_id[(*len)++] = (uint8_t)value;
            }
            else
            {
                // Other escape (like \n, \t, etc.)
                client_id[(*len)++] = *ptr++;
            }
        }
        else
        {
            // Regular character
            client_id[(*len)++] = *ptr++;
        }
    }

    return 0;
}

void format_client_id_to_string(const uint8_t *client_id, uint32_t len, char *output, size_t output_len)
{
    if (!client_id || !output || output_len < 3 || len == 0)
    {
        if (output && output_len > 0)
            output[0] = '\0';
        return;
    }

    char *ptr = output;
    size_t remaining = output_len;

    // Start with quote
    *ptr++ = '"';
    remaining--;

    // Convert each byte to octal escape
    for (uint32_t i = 0; i < len && remaining > 5; i++)
    {
        int written = snprintf(ptr, remaining, "\\%03o", client_id[i]);
        if (written < 0 || written >= (int)remaining)
            break;
        ptr += written;
        remaining -= written;
    }

    // End with quote
    if (remaining > 1)
    {
        *ptr++ = '"';
        *ptr = '\0';
    }
}
