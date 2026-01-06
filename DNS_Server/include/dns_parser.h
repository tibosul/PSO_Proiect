#ifndef DNS_PARSER_H
#define DNS_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include "dns_packet.h"

int parse_dns_request(const unsigned char* buffer, size_t len, char* qname, uint16_t* qtype);

#endif 