#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include <stdint.h>
#include <netinet/in.h>

/**
 * @brief Parse an IPv4 address string to in_addr structure
 * @param str IP address string (e.g., "192.168.1.1")
 * @param addr Output in_addr structure
 * @return 0 on success,
 *         -1 if str or addr is NULL,
 *         -2 if IP address format is invalid
 */
int parse_ip_address(const char *str, struct in_addr *addr);

/**
 * @brief Parse a MAC address string to byte array
 * @param str MAC address string (e.g., "00:11:22:33:44:55")
 * @param mac Output MAC address array (6 bytes)
 * @return 0 on success,
 *         -1 if str or mac is NULL,
 *         -2 if MAC address format is invalid
 */
int parse_mac_address(const char *str, uint8_t mac[6]);

/**
 * @brief Format a MAC address from byte array to string
 * @param mac MAC address array (6 bytes)
 * @param output Output string buffer
 * @param output_len Size of output buffer
 */
void format_mac_address(const uint8_t mac[6], char *output, size_t output_len);

/**
 * @brief Parse a comma-separated list of IP addresses
 * @param str Comma-separated IP list (e.g., "8.8.8.8, 8.8.4.4")
 * @param addrs Output array of in_addr structures
 * @param max_count Maximum number of addresses to parse
 * @return Number of addresses successfully parsed (>= 0) on success,
 *         -1 if str or addrs is NULL, or max_count <= 0,
 *         -2 if any IP address format is invalid,
 *         -3 if memory allocation fails
 */
int parse_ip_list(const char *str, struct in_addr *addrs, int max_count);

#endif // NETWORK_UTILS_H
