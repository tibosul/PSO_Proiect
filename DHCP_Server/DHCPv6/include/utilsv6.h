#ifndef UTILSV6_H
#define UTILSV6_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stddef.h>


/**
 * @file utilsv6.h
 * @brief IPv6 / DHCPv6 helper utilities.
 *
 * This module provides helpers for:
 * - IPv6 validation and conversion (string <-> binary)
 * - IPv6 comparisons and subnet membership checks
 * - Incrementing IPv6 addresses
 * - Parsing lists of IPv6 addresses from configuration strings
 * - Encoding domain names in RFC 1035 (DNS label) format
 * - Simple string sanitation helpers used by config/lease parsing
 * - DUID formatting utilities (binary -> hex with ':' separators)
 */

/* ===================== IPv6 helpers ===================== */

/**
 * @brief Validate an IPv6 address string.
 *
 * This function checks if the given IPv6 address string is valid.
 *
 * @param ip_str IPv6 address string to validate.
 * @return true if the IPv6 address string is valid, false otherwise.
 */
bool is_valid_ipv6(const char *ip_str);

/**
 * @brief Convert an IPv6 address string to binary.
 *
 * This function converts an IPv6 address string to binary format.
 *
 * @param ip_str IPv6 address string to convert.
 * @param addr   Output buffer to store the binary representation of the IPv6 address.
 * @return 0 on success, -1 on failure.
 */
int ipv6_str_to_bin(const char *ip_str, struct in6_addr *addr);

/**
 * @brief Convert an IPv6 address from binary to string.
 *
 * This function converts an IPv6 address from binary format to a string.
 *
 * @param addr   IPv6 address in binary format.
 * @param ip_str Output buffer to store the IPv6 address string.
 * @param size   Size of the output buffer in bytes.
 * @return Number of bytes written to the output buffer, or -1 on error.
 */
int ipv6_bin_to_str(const struct in6_addr *addr, char *ip_str, size_t size);

/**
 * @brief Compare two IPv6 addresses.
 *
 * This function compares two IPv6 addresses.
 *
 * @param addr1 First IPv6 address to compare.
 * @param addr2 Second IPv6 address to compare.
 * @return 0 if the two IPv6 addresses are equal, non-zero otherwise.
 */
int ipv6_compare(const struct in6_addr *addr1, const struct in6_addr *addr2);

/**
 * @brief Convert a list of IPv6 addresses from a string to binary.
 *
 * This function converts a list of IPv6 addresses from a string to binary format.
 *
 * @param str    String containing the list of IPv6 addresses.
 * @param out    Output buffer to store the binary representation of the IPv6 addresses.
 * @param max_count Maximum number of IPv6 addresses to convert.
 * @return Number of IPv6 addresses converted, or -1 on error.
 */
int str_to_ipv6_list(const char *str, struct in6_addr *out, size_t max_count);

/**
 * @brief Encode a domain name in RFC 1035 (DNS label) format.
 *
 * This function encodes a domain name in RFC 1035 (DNS label) format.
 *
 * @param domain Domain name to encode.
 * @param buf    Output buffer to store the encoded domain name.
 * @param len    Size of the output buffer in bytes.
 * @return Number of bytes written to the output buffer, or -1 on error.
 */
int encode_domain_name(const char *domain, uint8_t *buf, size_t len);

/**
 * @brief Check if an IPv6 address is in a subnet.
 *
 * This function checks if an IPv6 address is in a subnet.
 *
 * @param ip     IPv6 address to check.
 * @param subnet Subnet to check against.
 * @param prefix_len Prefix length for the subnet.
 * @return true if the IPv6 address is in the subnet, false otherwise.
 */
bool ipv6_in_subnet(const struct in6_addr *ip, const struct in6_addr *subnet, uint8_t prefix_len);

/**
 * @brief Increment an IPv6 address.
 *
 * This function increments an IPv6 address by one.
 *
 * @param ip IPv6 address to increment.
 * @return true if the IPv6 address was incremented successfully, false otherwise.
 */
bool ipv6_increment(struct in6_addr *ip);

/**
 * @brief Check if a MAC address string is valid.
 *
 * This function checks if the given MAC address string is valid.
 *
 * @param mac_str MAC address string to validate.
 * @return true if the MAC address string is valid, false otherwise.
 */
bool is_valid_mac(const char *mac_str);

/**
 * @brief Check if a DUID string is valid.
 *
 * This function checks if the given DUID string is valid.
 *
 * @param duid_str DUID string to validate.
 * @return true if the DUID string is valid, false otherwise.
 */
bool is_valid_duid(const char *duid_str);

/**
 * @brief Remove trailing whitespace from a string.
 *
 * This function removes trailing whitespace from a string.
 *
 * @param str String to remove trailing whitespace from.
 */
void rstrip(char *str);

/**
 * @brief Remove a semicolon from a string.
 *
 * This function removes a semicolon from a string.
 *
 * @param s String to remove a semicolon from.
 */
void strip_semicolon(char *s);

/**
 * @brief Skip leading whitespace in a string.
 *
 * This function skips leading whitespace in a string.
 *
 * @param s String to skip leading whitespace from.
 * @return Pointer to the first non-whitespace character in the string.
 */
char *lskip(char *s);

/**
 * @brief Remove quotes from a string.
 *
 * This function removes quotes from a string.
 *
 * @param s String to remove quotes from.
 */
void unquote(char *s);

/**
 * @brief Remove inline comments from a string.
 *
 * This function removes inline comments from a string.
 *
 * @param s String to remove inline comments from.
 */
void strip_inline_comment(char *s);

/**
 * @brief Parse a prefix and length from a string.
 *
 * This function parses a prefix and length from a string.
 *
 * @param in Input string containing the prefix and length.
 * @param prefix_out Output buffer to store the prefix.
 * @param prefix_out_sz Size of the output buffer in bytes.
 * @param len_out Output buffer to store the length.
 * @return Number of bytes written to the output buffer, or -1 on error.
 */
int parse_prefix_and_len(const char *in, char *prefix_out,size_t prefix_out_sz,uint8_t *len_out);

/**
 * @brief Convert a DUID from binary to hex.
 *
 * This function converts a DUID from binary to hex.
 *
 * @param duid Input DUID in binary format.
 * @param duid_len Length of the input DUID in bytes.
 * @param out Output buffer to store the hex representation of the DUID.
 * @param outsz Size of the output buffer in bytes.
 * @return Number of bytes written to the output buffer, or -1 on error.
 */
int duid_bin_to_hex(const uint8_t *duid, uint16_t duid_len, char *out,size_t outsz);
#endif