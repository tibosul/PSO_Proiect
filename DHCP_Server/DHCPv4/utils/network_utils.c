#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "../include/network_utils.h"
#include "../include/string_utils.h"

int parse_ip_address(const char* str, struct in_addr* addr)
{
    if(inet_pton(AF_INET, str, addr) == 1)
    {
        return 0;
    }
    return -1;
}

int parse_mac_address(const char* str, uint8_t mac[6])
{
    uint32_t values[6];
    if(sscanf(str, "%x:%x:%x:%x:%x:%x", &values[0], &values[1],
        &values[2], &values[3], &values[4], &values[5]) == 6)
    {
        for(int i = 0; i < 6; i++)
        {
            mac[i] = (uint8_t)values[i];
        }
        return 0;
    }
    return -1;
}

void format_mac_address(const uint8_t mac[6], char* output, size_t output_len)
{
    snprintf(output, output_len, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

int parse_ip_list(const char* str, struct in_addr* addrs, int max_count)
{
    char* str_copy = strdup(str);
    char* token;
    char* saveptr;
    int count = 0;

    token = strtok_r(str_copy, ",", &saveptr);
    while(token && count < max_count)
    {
        token = trim(token);
        if(parse_ip_address(token, &addrs[count]) == 0)
        {
            count++;
        }
        token = strtok_r(NULL, ",", &saveptr);
    }

    free(str_copy);
    return count;
}
