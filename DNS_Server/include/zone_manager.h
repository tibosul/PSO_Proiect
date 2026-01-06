#ifndef ZONE_MANAGER_H
#define ZONE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dns_config.h"

typedef struct zone_record{
    char name[256]; // nume (ex: www.mta.ro)
    uint16_t type; // tip
    uint32_t TTL; // Time to live
    char rdata[256]; // (adresa ip sau hostname)
    struct zone_record *next;
}zone_record;

typedef struct zone_node{
    char origin[256]; // nume zona
    zone_record *records; // lista inregistrari
    struct zone_node *next;
}zone_node;

void zone_manager_init(config_node* config_root);

bool handle_local_zone_query(const char* qname, uint16_t qtype, const unsigned char* query_packet, size_t query_len, unsigned char* response_packet, size_t* response_len);

void load_zone_from_file(zone_node* zone, const char* filename);

void parse_zone_line(zone_node* zone, char* line, char* last_domain, uint32_t* current_ttl);

#endif 