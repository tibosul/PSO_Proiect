#include "lease_v4.h"
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define MAX_LINE_LEN 1024

static char* trim(char* str)
{
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    
    char* end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

const char* lease_state_to_string(lease_state_t state)
{
    switch(state)
    {
        case LEASE_STATE_FREE: return "free";
        case LEASE_STATE_ACTIVE: return "active";
        case LEASE_STATE_EXPIRED: return "expired";
        case LEASE_STATE_RELEASED: return "released";
        case LEASE_STATE_ABANDONED: return "abandoned";
        case LEASE_STATE_RESERVED: return "reserved";
        default: return "unknown";
    }
}

lease_state_t lease_state_from_string(const char* str)
{
    if(strcmp(str, "free") == 0) return LEASE_STATE_FREE;
    if(strcmp(str, "active") == 0) return LEASE_STATE_ACTIVE;
    if(strcmp(str, "expired") == 0) return LEASE_STATE_EXPIRED;
    if(strcmp(str, "released") == 0) return LEASE_STATE_RELEASED;
    if(strcmp(str, "abandoned") == 0) return LEASE_STATE_ABANDONED;
    if(strcmp(str, "reserved") == 0) return LEASE_STATE_RESERVED;

    // If neither:
    return LEASE_STATE_FREE;
}

bool lease_is_expired(const struct dhcp_lease_t* lease)
{
    time_t now = time(NULL);
    return (lease->state == LEASE_STATE_ACTIVE && lease->end_time < now);
}

int lease_db_init(struct lease_database_t* db, const char* filename)
{
    if(!db || !filename) return -1;

    memset(db, 0, sizeof(struct lease_database_t));
    strncpy(db->filename, filename, sizeof(db->filename) - 1);

    return 0;
}

void lease_db_free(struct lease_database_t* db)
{
    if(db)
    {
        memset(db, 0, sizeof(struct lease_database_t));
    }
}

static int parse_mac_from_lease(const char* str, uint8_t mac[6])
{
    uint32_t values[6];
    if(sscanf(str, "%x:%x:%x:%x:%x:%x", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]) == 0)
    {
        for(int i = 0; i < 6; i++)
        {
            mac[i] = (uint8_t)values[i];
        }
        return 0;
    }
    return -1;
}

static int parse_lease_block(FILE* fp, struct dhcp_lease_t* lease, char* first_line)
{
    memset(lease, 0, sizeof(struct dhcp_lease_t));

    // Parse "lease x.x.x.x {"
    char* token = strtok(first_line, " \t{");
    if(!token || strcmp(token, "lease") != 0) return -1;

    char* ip_str = strtok(NULL, " \t{");
    if(!ip_str || inet_pton(AF_INET, ip_str, &lease->ip_address) != 1)
    {
        return -1;
    }

    // Default state
    lease->state = LEASE_STATE_FREE;

    char line[MAX_LINE_LEN];
    while(fgets(line, sizeof(line), fp))
    {
        char* trimmed = trim(line);

        // Skip empty lines and comments
        if(strlen(trimmed) == 0 || trimmed[0] == '#') continue;
        if(trimmed[0] == '}')
        {
            return 0;
        }

        char* key = strtok(trimmed, " \t");
        if(!key) continue;

        if(strcmp(key, "starts") == 0)
        {
            char* value = strtok(NULL, ";");
            if(value) lease->start_time = (time_t)atoll(trim(value));
        }
        else if(strcmp(key, "ends") == 0)
        {
            char* value = strtok(NULL, ";");
            if(value) lease->end_time = (time_t)atoll(trim(value));
        }
        else if(strcmp(key, "hardware") == 0)
        {
            char* hw_type = strtok(NULL, " \t"); // "ethernet"
            char* mac_str = strtok(NULL, ";");
            if(mac_str)
            {
                parse_mac_from_lease(trim(mac_str), lease->mac_address);
            }
        }
        else if(strcmp(key, "client-hostname") == 0)
        {
            char* hostname = strtok(NULL, ";");
            if(hostname)
            {
                hostname = trim(hostname);
                
                // Remove quotes
                if(hostname[0] == '"') hostname++;
                int len = strlen(hostname);
                if(len > 0 && hostname[len - 1] == '"') hostname[len - 1] = '\0';
                strncpy(lease->client_hostname, hostname, MAX_CLIENT_HOSTNAME - 1);
            }
        }
        else if(strcmp(key, "binding") == 0)
        {
            char* binding_key = strtok(NULL, " \t");
            if(binding_key && strcmp(binding_key, "state") == 0)
            {
                char* state_str = strtok(NULL, ";");
                if(state_str)
                {
                    state_str = trim(state_str);
                    lease->state = lease_state_from_string(state_str);
                    strncpy(lease->binding_state, state_str, sizeof(lease->binding_state) - 1);
                }
            }
        }
    }
    return -1;
}

int lease_db_load(struct lease_database_t* db)
{
    if(!db) return -1;

    FILE* fp = fopen(db->filename, "r");
    if(!fp)
    {
        printf("Lease file %s not found, starting with empty database\n", db->filename);
        return 0;
    }

    db->lease_count = 0;

    char line[MAX_LINE_LEN];
    while(fgets(line, sizeof(line), fp))
    {
        char* trimmed = trim(line);

        // Skip empty lines and comments
        if(strlen(trimmed) == 0 || trimmed[0] == '#') continue;

        // Look for lease blocks
        if(strncmp(trimmed, "lease", 5) == 0)
        {
            if(db->lease_count < MAX_LEASES)
            {
                if(parse_lease_block(fp, &db->leases[db->lease_count], trimmed) == 0)
                {
                    db->lease_count++;
                }
            }
        }
    }

    fclose(fp);
    printf("Loaded %u leases from %s\n", db->lease_count, db->filename);
    return 0;
}

int lease_db_save(struct lease_database_t* db)
{
    if(!db) return -1;

    FILE* fp = fopen(db->filename, "w");
    if(!fp)
    {
        perror("Failed to open lease file for writing\n");
        return -1;
    }
    fprintf(fp, "# DHCP lease database\n");
    fprintf(fp, "# This file is automatically generated, do not edit manually\n");
    time_t now = time(NULL);
    fprintf(fp, "# Last updated: %s\n", ctime(&now));

    for(uint32_t i = 0; i < db->lease_count; i++)
    {
        struct dhcp_lease_t* lease = &db->leases[i];
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &lease->ip_address, ip_str, INET6_ADDRSTRLEN);

        fprintf(fp, "\nlease %s {\n", ip_str);
        fprintf(fp, "\tstarts %ld;\n", lease->start_time);
        fprintf(fp, "\tends %ld;\n", lease->end_time);
        fprintf(fp, "\thardware ethernet %02x:%02x:%02x:%02x:%02x:%02x",
                lease->mac_address[0], lease->mac_address[1], lease->mac_address[2],
                lease->mac_address[3], lease->mac_address[4], lease->mac_address[5]);
        
        if(strlen(lease->client_hostname) > 0)
        {
            fprintf(fp, "\tclient-hostname \"%s\";\n", lease->client_hostname);
        }
        fprintf(fp, "}\n");
    }
    fclose(fp);
    return 0;
}

int lease_db_append_lease(struct lease_database_t* db, const struct dhcp_lease_t* lease)
{
    if(!db || !lease) return -1;

    FILE* fp = fopen(db->filename, "a");
    if(!fp)
    {
        perror("Failed to open lease file for appending\n");
        return -1;
    }

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &lease->ip_address, ip_str, INET_ADDRSTRLEN);

    fprintf(fp, "\nlease %s {\n", ip_str);
    fprintf(fp, "\tstarts %ld;\n", lease->start_time);
    fprintf(fp, "\tends %ld;\n", lease->end_time);
    fprintf(fp, "hardware ethernet %02x:%02x:%02x:%02x:%02x:%02x",
            lease->mac_address[0], lease->mac_address[1], lease->mac_address[2],
            lease->mac_address[3], lease->mac_address[4], lease->mac_address[5]);

    if(strlen(lease->client_hostname) > 0)
    {
        fprintf(fp, "client-hostname \"%s\";\n", lease->client_hostname);
    }

    fprintf(fp, "binding state %s;\n", lease_state_to_string(lease->state));
    fprintf(fp, "}\n");

    fclose(fp);
    return 0;
}

struct dhcp_lease_t* lease_db_add_lease(struct lease_database_t* db, struct in_addr ip, const uint8_t mac[6], uint32_t lease_time)
{
    if(!db || db->lease_count >= MAX_LEASES) return NULL;

    struct dhcp_lease_t* lease = &db->leases[db->lease_count];
    memset(lease, 0, sizeof(struct dhcp_lease_t));

    lease->ip_address = ip;
    memcpy(lease->mac_address, mac, 6);
    lease->start_time = time(NULL);
    lease->end_time = lease->start_time + lease_time;
    lease->state = LEASE_STATE_ACTIVE;

    db->lease_count++;
    lease_db_append_lease(db, lease);

    return lease;
}

struct dhcp_lease_t* lease_db_find_by_ip(struct lease_database_t* db, struct in_addr ip)
{
    if(!db) return NULL;

    for(uint32_t i = 0; i < db->lease_count; i++)
    {
        if(db->leases[i].ip_address.s_addr == ip.s_addr)
        {
            return &db->leases[i];
        }
    }
    return NULL;
}

struct dhcp_lease_t* lease_db_find_by_mac(struct lease_database_t* db, const uint8_t mac[6])
{
    if(!db) return NULL;

    for(uint32_t i = 0; i < db->lease_count; i++)
    {
        if(memcmp(db->leases[i].mac_address, mac, 6) == 0)
        {
            return &db->leases[i];
        }
    }
    return NULL;
}

int lease_db_release_lease(struct lease_database_t* db, struct in_addr ip)
{
    struct dhcp_lease_t* lease = lease_db_find_by_ip(db, ip);
    if(!lease) return -1;

    lease->state = LEASE_STATE_RELEASED;
    lease->end_time = time(NULL); // Mark as ended now

    lease_db_save(db);
    return 0;
}

int lease_db_renew_lease(struct lease_database_t *db, struct in_addr ip, uint32_t lease_time)
{
    struct dhcp_lease_t* lease = lease_db_find_by_ip(db, ip);
    if(!lease) return -1;

    time_t now = time(NULL);
    lease->start_time = now;
    lease->end_time = now + lease_time;
    lease->state = LEASE_STATE_ACTIVE;

    lease_db_save(db);
    return 0;
}

int lease_db_expire_old_leases(struct lease_database_t *db)
{
    if(!db) return -1;
    
    uint32_t expired_count = 0;
    time_t now = time(NULL);

    for(uint32_t i = 0; i < db->lease_count; i++)
    {
        struct dhcp_lease_t* lease = &db->leases[i];
        if(lease->state == LEASE_STATE_ACTIVE && lease->end_time < now)
        {
            lease->state = LEASE_STATE_EXPIRED;
            expired_count++;
        }
    }

    if(expired_count > 0)
    {
        lease_db_save(db);
    }

    return expired_count;
}

int db_lease_cleanup_expired(struct lease_database_t* db)
{
    if(!db) return -1;

    uint32_t removed = 0;

    uint32_t i = 0;
    while(i < db->lease_count)
    {
        struct dhcp_lease_t* lease = &db->leases[i];

        if(lease->state == LEASE_STATE_EXPIRED || lease->state == LEASE_STATE_RELEASED)
        {
            memmove(&db->leases[i], &db->leases[i + 1], (db->lease_count - i - 1) * sizeof(struct dhcp_lease_t));
            db->lease_count--;
            removed++;
        }
        else
        {
            i++;
        }
    }
    
    if(removed > 0)
    {
        lease_db_save(db);
    }

    return removed;
}

void lease_db_print(const struct lease_database_t *db)
{
    if(!db) return;
    
    printf("--- Lease Database ---\n");
    printf("File: %s\n", db->filename);
    printf("Total Leases: %u\n\n", db->lease_count);
    
    for(uint32_t i = 0; i < db->lease_count; i++)
    {
        const struct dhcp_lease_t *lease = &db->leases[i];
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &lease->ip_address, ip_str, INET_ADDRSTRLEN);
        
        printf("Lease %u:\n", i+1);
        printf("  IP: %s\n", ip_str);
        printf("  MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
               lease->mac_address[0], lease->mac_address[1], lease->mac_address[2],
               lease->mac_address[3], lease->mac_address[4], lease->mac_address[5]);
        printf("  State: %s\n", lease_state_to_string(lease->state));
        printf("  Start: %s", ctime(&lease->start_time));
        printf("  End: %s", ctime(&lease->end_time));
        
        if(strlen(lease->client_hostname) > 0)
        {
            printf("  Hostname: %s\n", lease->client_hostname);
        }
        
        if(lease_is_expired(lease))
        {
            printf("  *** EXPIRED ***\n");
        }
        
        printf("\n");
    }
}
