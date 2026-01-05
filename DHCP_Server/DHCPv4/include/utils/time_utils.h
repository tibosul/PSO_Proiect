#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <time.h>
#include <stddef.h>

/**
 * @brief Parse ISC DHCP time format to Unix timestamp
 * @param time_str Time string in ISC DHCP format (e.g., "4 2024/10/26 14:30:00")
 * @return Unix timestamp, or parsed epoch if not in ISC format
 */
time_t parse_lease_time(const char *time_str);

/**
 * @brief Format Unix timestamp to ISC DHCP time format
 * @param timestamp Unix timestamp
 * @param output Output string buffer
 * @param output_len Size of output buffer
 */
void format_lease_time(time_t timestamp, char *output, size_t output_len);

#endif // TIME_UTILS_H
