#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <stdio.h>

/**
 * @brief Advance the file pointer to the next closing brace '}'.
 * @param fp File pointer to read from.
 * @return 0 on success,
 *         -1 if fp is NULL or an error occurs
 */
int advance_to_next_closed_brace(FILE *fp);

#endif // FILE_UTILS_H