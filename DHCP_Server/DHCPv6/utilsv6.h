#ifndef UTILSV6_H
#define UTILSV6_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stddef.h>

bool is_valid_ipv6(const char *ip_str);
int ipv6_str_to_bin(const char *ip_str, struct in6_addr *addr);
int ipv6_bin_to_str(const struct in6_addr *addr, char *ip_str, size_t size);

int ipv6_compare(const struct in6_addr *addr1, const struct in6_addr *addr2);
bool ipv6_in_subnet(const struct in6_addr *ip, const struct in6_addr *subnet, uint8_t prefix_len);

bool ipv6_increment(struct in6_addr *ip);

bool is_valid_mac(const char *mac_str);
bool is_valid_duid(const char *duid_str);

#endif