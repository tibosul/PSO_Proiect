#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <stdio.h>

/**
 * @brief Advance the file pointer to the next closing brace '}' without a matching opening brace '{' before it.
 * @param fp File pointer to read from.
 * @return 0 on success,
 *         -1 if fp is NULL or an error occurs
 *
 * Example usage: If parsing a configuration file and you want to skip to the end of a block.
 */
int advance_to_next_closed_brace(FILE *fp);

#endif // FILE_UTILS_H
