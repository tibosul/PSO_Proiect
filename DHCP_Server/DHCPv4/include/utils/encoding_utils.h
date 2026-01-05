#ifndef ENCODING_UTILS_H
#define ENCODING_UTILS_H

#include <stdint.h>
#include <stddef.h>

#define MAX_CLIENT_ID_LEN 64

/**
 * @brief Parse client ID from ISC DHCP octal-escaped string format
 * @param str Input string (e.g., "\001\000\021\042\063\104\125\252")
 * @param client_id Output buffer for client ID bytes
 * @param len Output length of parsed client ID
 * @return 0 on success, -1 on failure
 */
int parse_client_id_from_string(const char *str, uint8_t *client_id, uint32_t *len);

/**
 * @brief Format client ID to ISC DHCP octal-escaped string format
 * @param client_id Input client ID bytes
 * @param len Length of client ID
 * @param output Output string buffer
 * @param output_len Size of output buffer
 */
void format_client_id_to_string(const uint8_t *client_id, uint32_t len, char *output, size_t output_len);

#endif // ENCODING_UTILS_H
