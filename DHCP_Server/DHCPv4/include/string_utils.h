#ifndef STRING_UTILS_H
#define STRING_UTILS_H

/**
 * @brief Trim whitespace from both ends of a string
 * @param str String to trim (modified in place)
 * @return Pointer to trimmed string
 */
char* trim(char* str);

/**
* @brief Remove quotes from both ends of a string
* @param str String to remove quotes from (modified in place)
* @return Pointer to modified string
*/
char* remove_quotes(char* str);

#endif // STRING_UTILS_H
