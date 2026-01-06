#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <arpa/inet.h>

int initialize_udp_socket(const char* ip, uint16_t port);

size_t forward_to_upstream(const char* upstream_ip, const unsigned char* query_buf, size_t query_len, unsigned char* response_buf, int timeout_seconds);

#endif