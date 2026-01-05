#ifndef STRING_UTILS_H
#define STRING_UTILS_H

/**
 * @brief Trim whitespace from both ends of a string
 * @param str String to trim (modified in place)
 * @return Pointer to trimmed string, or NULL if str is NULL
 */
char *trim(char *str);

/**
 * @brief Remove quotes from both ends of a string
 * @param str String to remove quotes from (modified in place)
 * @return Pointer to modified string, or NULL if str is NULL
 */
char *remove_quotes(char *str);

/**
 * @brief Parse an unsigned integer from string
 * @param str String representation of unsigned integer
 * @param value Pointer to uint32_t to populate
 * @return 0 on success,
 *         -1 if str or value is NULL,
 *         -2 if string format is invalid or conversion fails,
 *         -3 if value exceeds UINT32_MAX
 */
int parse_uint32(const char *str, uint32_t *value);

#endif // STRING_UTILS_H
