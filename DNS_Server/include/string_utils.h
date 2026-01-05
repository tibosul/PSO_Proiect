#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <stddef.h>

void dns_binary_to_text(unsigned char* read_pointer, unsigned char* buffer, int* count, char* output);

void text_to_dns_binary(unsigned char* domain_name, unsigned char* host);

#endif