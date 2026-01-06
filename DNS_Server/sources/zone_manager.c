#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>
#include "zone_manager.h"
#include "dns_packet.h"
#include "string_utils.h"

#define MAX_LINE_LEN 1024

static zone_node* global_zones = NULL;
static char global_zones_dir[512] = ".";

static const char* get_config_value(config_pair* pairs, const char* key)
{
    if(pairs == NULL)
    {
        return NULL;
    }

    for(int i=0; pairs[i].key != NULL; i++)
    {
        if(strcmp(pairs[i].key, key) == 0)
        {
            return pairs[i].value;
        }
    }

    return NULL;
}

static void add_record_to_zone(zone_node* zone, const char* name, uint16_t type, uint32_t ttl, const char* rdata)
{
    zone_record* new_record = (zone_record*)malloc(sizeof(zone_record));

    if(new_record == NULL)
    {
        perror("Error: Failed to allocate memory for a new zone record!\n");
        return;
    }

    strncpy(new_record->name, name, sizeof(new_record->name) - 1);
    new_record->name[sizeof(new_record->name) - 1] = '\0';
    new_record->type = type;
    new_record->TTL = ttl;
    strncpy(new_record->rdata, rdata, sizeof(new_record->rdata) - 1);
    new_record->rdata[sizeof(new_record->rdata) - 1] = '\0';

    new_record->next = zone->records;
    zone->records = new_record;
}

void zone_manager_init(config_node* config_root)
{
    if(config_root == NULL) return;

    config_node* current_node = config_root;

    while(current_node != NULL) 
    {
        if(current_node->type == CONFIG_OPTIONS) 
        {
            const char* zdir = get_config_value(current_node->pairs, "zones_dir");
            
            if(zdir != NULL) 
            {
                strncpy(global_zones_dir, zdir, sizeof(global_zones_dir) - 1);
                global_zones_dir[sizeof(global_zones_dir) - 1] = '\0';

                printf("Zones directory set to: %s\n", global_zones_dir);
            }

            break;
        }

        current_node = current_node->next;
    }

    current_node = config_root;

    while (current_node != NULL) 
    {
        if(current_node->type == CONFIG_ZONE && current_node->name != NULL) 
        {
            
            zone_node* new_zone = (zone_node*)malloc(sizeof(zone_node));
            if(new_zone == NULL)
            {
                perror("Error: Failed to allocate memory for a new zone!\n");
                return;
            }

            strncpy(new_zone->origin, current_node->name, sizeof(new_zone->origin) - 1);
            new_zone->origin[sizeof(new_zone->origin) - 1] = '\0';
            new_zone->records = NULL;
            new_zone->next = NULL;

            const char* type = get_config_value(current_node->pairs, "type");
            const char* file = get_config_value(current_node->pairs, "file");

            if(type && strcmp(type, "master") == 0 && file) 
            {
                printf("Loading zone '%s' from file '%s'...\n", new_zone->origin, file);
                load_zone_from_file(new_zone, file);
            }else {
                printf("Warning: Zone '%s' incomplete config or not master.\n", new_zone->origin);
            }

            new_zone->next = global_zones;
            global_zones = new_zone;
        }
        current_node = current_node->next;
    }    
}

void parse_zone_line(zone_node* zone, char* line, char* last_domain, uint32_t* current_ttl)
{
    char* ptr = line;

    while(*ptr && isspace((unsigned char)*ptr))
    {
        ptr++;
    }

    if(*ptr == '\0' || *ptr == ';' || *ptr == '#')
    {
        return;
    }

    if(strncmp(ptr, "$TTL", 4) == 0)
    {
        ptr = ptr + 4;

        while(*ptr && isspace((unsigned char)*ptr))
        {
            ptr++;
        }

        *current_ttl = (uint32_t)atoi(ptr);
        return;
    }

    char name[256] = {0};
    char token[256];
    int offset = 0;

    if(isspace((unsigned char)line[0]))
    {
        strncpy(name, last_domain, sizeof(name) - 1);
    }else {
        int n = 0;
        sscanf(ptr, "%s%n", name, &n);
        ptr = ptr + n;

        if(strcmp(name, "@") == 0)
        {
            strncpy(name, zone->origin, sizeof(name) - 1);
        }

        strncpy(last_domain, name, sizeof(name) - 1);
        last_domain[sizeof(last_domain) - 1] = '\0';
    }

    uint16_t type = 0;
    char rdata[256] = {0};
    uint32_t ttl = *current_ttl;

    while(*ptr != NULL)
    {
        while(*ptr != NULL && isspace((unsigned char)*ptr))
        {
            ptr++;

            if(*ptr == '\0' || *ptr == ';')
            {
                break;
            }
        }   

        int n = 0;
        sscanf(ptr, "%s%n", token, &n);
        ptr = ptr + n;

        if(isdigit((unsigned char)token[0]))
        {
            ttl = (uint32_t)atoi(token);
            continue;
        }

        if(strcmp(token, "IN") == 0)
        {
            continue;
        }

        if(strcmp(token, "A") == 0)
        {
            type = 1;
        }
        else if(strcmp(token, "NS") == 0)
        {
            type = 2;
        }
        else if(strcmp(token, "CNAME") == 0)
        {
            type = 5;
        }
        else if(strcmp(token, "SOA") == 0) // Start of [a zone of] authority record 
        {
            //TODO: implementare parsare SOA
            type = 6; 
            break;
        }
        else if(strcmp(token, "PTR") == 0) // Pointer to a canonical name 
        {
            type = 12;
        }
        else if(strcmp(token, "MX") == 0)
        {
            type = 15;
        }
        else if(strcmp(token, "AAAA") == 0)
        {
            type = 28;
        }

        if(type != 0)
        {
            while(*ptr && isspace((unsigned char)*ptr))
            {
                ptr++;
            }

            if(type == 6)
            {
                //TODO: implementare parsare SOA
                return;
            }

            int r_idx = 0;
            while(*ptr && *ptr != ';' && *ptr != '\n')
            {
                rdata[r_idx++] = *ptr++;
            }

            while(r_idx > 0 && isspace((unsigned char)rdata[r_idx - 1]))
            {
                r_idx--;
            }

            rdata[r_idx] = '\0';

            add_record_to_zone(zone, name, type, ttl, rdata);
            return;
        }

    }
}

void load_zone_from_file(zone_node* zone, const char* filename)
{
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s", global_zones_dir, filename);

    FILE* file = fopen(filepath, "r");
    if(file == NULL)
    {
        perror("Error while trying to open zone file!\n");
        return;
    }

    char line[MAX_LINE_LEN];
    char last_domain[256];

    strncpy(last_domain, zone->origin, sizeof(last_domain) - 1);

    uint32_t current_ttl = 3600;

    while(fgets(line, sizeof(line), file))
    {
        line[strcspn(line, "\r\n")] = 0; // eliminare newline si carriage return
        parse_zone_line(zone, line, last_domain, &current_ttl);
    }

    fclose(file);
}


bool handle_local_zone_query(const char* qname, uint16_t qtype, const unsigned char* query_packet, size_t query_len, unsigned char* response_packet, size_t* response_len)
{
    zone_node* current_zone = global_zones;
    
    char qname_dot[257];
    snprintf(qname_dot, sizeof(qname_dot), "%s.", qname);
    if (qname[strlen(qname)-1] == '.') 
    {
        strcpy(qname_dot, qname);
    }

    while (current_zone != NULL)
    {
        zone_record* current_record = current_zone->records;

        while (current_record != NULL)
        {
            bool match_name = false;
            
            if (strcmp(qname, current_record->name) == 0) 
            {
                match_name = true;
            }
            
            if (strcmp(qname_dot, current_record->name) == 0) 
            {
                match_name = true;
            }

            if (match_name && (qtype == current_record->type || qtype == 255))
            {
                memcpy(response_packet, query_packet, query_len);

                dns_header* header = (dns_header*)response_packet;
                header->QR = 1;      
                header->AA = 1;      
                header->Rcode = 0;   
                header->TC = 0;      
                header->number_of_answers = htons(1);

                size_t offset = query_len;

                // Compresie nume
                response_packet[offset++] = 0xc0;
                response_packet[offset++] = 0x0c;

                resource_record_fixed* rr = (resource_record_fixed*)(response_packet + offset);
                rr->query_type = htons(current_record->type);
                rr->query_class = htons(1); 
                rr->TTL = htonl(current_record->TTL);

                offset += sizeof(resource_record_fixed);

                // RDATA 
                if (current_record->type == 1) // A -> inregistrare adresa IPv4
                {
                    rr->data_length = htons(4);
                    struct in_addr addr;
                    if (inet_pton(AF_INET, current_record->rdata, &addr) > 0) 
                    {
                        memcpy(response_packet + offset, &addr.s_addr, 4);
                        offset = offset + 4;
                    }else {
                        rr->data_length = htons(0);
                    }
                }
                else if (current_record->type == 28) // AAAA -> inregistrare IPv6
                {
                    rr->data_length = htons(16); // IPv6 are 16 octeti 
                    struct in6_addr addr6;
                    
                    if (inet_pton(AF_INET6, current_record->rdata, &addr6) > 0)
                    {
                        // adresa valida
                        memcpy(response_packet + offset, &addr6, 16);
                        offset += 16;
                    } else {
                        // adresa invalida
                        rr->data_length = htons(0);
                    }
                }
                else if (current_record->type == 5 || current_record->type == 2 || current_record->type == 12) // CNAME, NS, PTR
                {
                    // conversie nume domeniu -> dns binary labels
                    unsigned char encoded_name[256];
                    text_to_dns_binary(encoded_name, (unsigned char*)current_record->rdata);
                    
                    int encoded_size = 0;
                    unsigned char* ptr = encoded_name;
                    while(*ptr != 0) {
                        int label_len = *ptr;
                        encoded_size += (label_len + 1);
                        ptr += (label_len + 1);
                    }
                    encoded_size++; 

                    rr->data_length = htons(encoded_size);
                    memcpy(response_packet + offset, encoded_name, encoded_size);
                    offset += encoded_size;
                }
                else {
                    rr->data_length = htons(0);
                }

                *response_len = offset;
                return true;
            }
            current_record = current_record->next;
        }
        current_zone = current_zone->next;
    }

    return false;
}