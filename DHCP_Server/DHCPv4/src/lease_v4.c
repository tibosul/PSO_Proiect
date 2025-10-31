#define _GNU_SOURCE
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
//#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include "../include/lease_v4.h"
#include "../include/string_utils.h"
#include "../include/network_utils.h"
#include "../include/time_utils.h"
#include "../include/encoding_utils.h"

#define MAX_LINE_LEN 1024

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
        case LEASE_STATE_BACKUP: return "backup";
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
    if(strcmp(str, "backup") == 0) return LEASE_STATE_BACKUP;

    // Default to free if unknown
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

    // Default state and transitions
    lease->state = LEASE_STATE_FREE;
    lease->next_binding_state = LEASE_STATE_FREE;
    lease->rewind_binding_state = LEASE_STATE_FREE;

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
            if(value)
            {
                lease->start_time = parse_lease_time(trim(value));
            }
        }
        else if(strcmp(key, "ends") == 0)
        {
            char* value = strtok(NULL, ";");
            if(value)
            {
                lease->end_time = parse_lease_time(trim(value));
            }
        }
        else if(strcmp(key, "tstp") == 0)
        {
            char* value = strtok(NULL, ";");
            if(value)
            {
                lease->tstp = parse_lease_time(trim(value));
            }
        }
        else if(strcmp(key, "cltt") == 0)
        {
            char* value = strtok(NULL, ";");
            if(value)
            {
                lease->cltt = parse_lease_time(trim(value));
            }
        }
        else if(strcmp(key, "hardware") == 0)
        {
            strtok(NULL, " \t"); // Skip hardware type (e.g., "ethernet")
            char* mac_str = strtok(NULL, ";");
            if(mac_str)
            {
                parse_mac_address(trim(mac_str), lease->mac_address);
            }
        }
        else if(strcmp(key, "uid") == 0)
        {
            char* uid_str = strtok(NULL, ";");
            if(uid_str)
            {
                parse_client_id_from_string(trim(uid_str), lease->client_id, &lease->client_id_len);
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
        else if(strcmp(key, "vendor-class-identifier") == 0)
        {
            char* vendor = strtok(NULL, ";");
            if(vendor)
            {
                vendor = trim(vendor);

                // Remove quotes if present
                if(vendor[0] == '"') vendor++;
                int len = strlen(vendor);
                if(len > 0 && vendor[len - 1] == '"') vendor[len - 1] = '\0';
                strncpy(lease->vendor_class_identifier, vendor, MAX_VENDOR_CLASS_LEN - 1);
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
        else if(strcmp(key, "next") == 0)
        {
            char* next_key = strtok(NULL, " \t");
            if(next_key && strcmp(next_key, "binding") == 0)
            {
                char* state_key = strtok(NULL, " \t");
                if(state_key && strcmp(state_key, "state") == 0)
                {
                    char* state_str = strtok(NULL, ";");
                    if(state_str)
                    {
                        lease->next_binding_state = lease_state_from_string(trim(state_str));
                    }
                }
            }
        }
        else if(strcmp(key, "rewind") == 0)
        {
            char* rewind_key = strtok(NULL, " \t");
            if(rewind_key && strcmp(rewind_key, "binding") == 0)
            {
                char* state_key = strtok(NULL, " \t");
                if(state_key && strcmp(state_key, "state") == 0)
                {
                    char* state_str = strtok(NULL, ";");
                    if(state_str)
                    {
                        lease->rewind_binding_state = lease_state_from_string(trim(state_str));
                    }
                }
            }
        }
        else if(strcmp(key, "abandoned;") == 0 || strcmp(key, "abandoned") == 0)
        {
            lease->is_abandoned = true;
        }
        else if(strcmp(key, "set") == 0)
        {
            // Parse vendor-specific options like: set vendor-string = "Cisco";
            // For now, we'll skip these
            continue;
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
        perror("Failed to open lease file for writing");
        return -1;
    }

    // Write file header
    fprintf(fp, "# The format of this file is documented in the dhcpd.leases(5) manual page.\n");
    fprintf(fp, "# This lease file was written by DHCPv4 Server\n");
    fprintf(fp, "#\n");
    fprintf(fp, "# authoring-byte-order entry is generated, DO NOT DELETE\n");
    fprintf(fp, "authoring-byte-order little-endian;\n\n");

    // Write server DUID (DHCP Unique Identifier)
    fprintf(fp, "# Server duid (DHCP Unique Identifier)\n");
    fprintf(fp, "server-duid \"\\000\\001\\000\\001\\053\\377\\214\\372\\000\\014\\051\\132\\173\\254\";\n\n");

    // Write format documentation
    fprintf(fp, "#########################################################################\n");
    fprintf(fp, "# Lease Database Format\n");
    fprintf(fp, "#########################################################################\n");
    fprintf(fp, "# lease <ip-address> { ... }\n");
    fprintf(fp, "#   starts <epoch|date>;              - When lease started\n");
    fprintf(fp, "#   ends <epoch|date>;                - When lease expires\n");
    fprintf(fp, "#   tstp <epoch>;                     - Time State was Put (last state change)\n");
    fprintf(fp, "#   cltt <epoch>;                     - Client Last Transaction Time\n");
    fprintf(fp, "#   binding state <state>;            - Current state: active, free, abandoned, etc.\n");
    fprintf(fp, "#   next binding state <state>;       - State after transition\n");
    fprintf(fp, "#   hardware ethernet <mac>;          - Client MAC address\n");
    fprintf(fp, "#   uid <hex-string>;                 - Client identifier (option 61)\n");
    fprintf(fp, "#   client-hostname \"<hostname>\";     - Client's hostname\n");
    fprintf(fp, "#   vendor-class-identifier \"<vci>\";  - Vendor identification\n");
    fprintf(fp, "#   set vendor-string = \"<string>\";   - Vendor specific info\n");
    fprintf(fp, "#########################################################################\n\n");

    time_t now = time(NULL);
    fprintf(fp, "# Last updated: %s\n", ctime(&now));

    // Write all leases
    for(uint32_t i = 0; i < db->lease_count; i++)
    {
        struct dhcp_lease_t* lease = &db->leases[i];
        char ip_str[INET_ADDRSTRLEN];
        char time_buf[64];

        inet_ntop(AF_INET, &lease->ip_address, ip_str, INET_ADDRSTRLEN);

        fprintf(fp, "\nlease %s {\n", ip_str);

        // Write timestamps in ISC DHCP format
        format_lease_time(lease->start_time, time_buf, sizeof(time_buf));
        fprintf(fp, "\tstarts %s;\n", time_buf);

        format_lease_time(lease->end_time, time_buf, sizeof(time_buf));
        fprintf(fp, "\tends %s;\n", time_buf);

        if(lease->tstp > 0)
        {
            format_lease_time(lease->tstp, time_buf, sizeof(time_buf));
            fprintf(fp, "\ttstp %s;\n", time_buf);
        }

        if(lease->cltt > 0)
        {
            format_lease_time(lease->cltt, time_buf, sizeof(time_buf));
            fprintf(fp, "\tcltt %s;\n", time_buf);
        }

        // Write binding states
        fprintf(fp, "\tbinding state %s;\n", lease_state_to_string(lease->state));

        if(lease->next_binding_state != LEASE_STATE_FREE)
        {
            fprintf(fp, "\tnext binding state %s;\n", lease_state_to_string(lease->next_binding_state));
        }

        if(lease->rewind_binding_state != LEASE_STATE_FREE)
        {
            fprintf(fp, "\trewind binding state %s;\n", lease_state_to_string(lease->rewind_binding_state));
        }

        // Write hardware address
        fprintf(fp, "\thardware ethernet %02x:%02x:%02x:%02x:%02x:%02x;\n",
                lease->mac_address[0], lease->mac_address[1], lease->mac_address[2],
                lease->mac_address[3], lease->mac_address[4], lease->mac_address[5]);

        // Write client ID if present
        if(lease->client_id_len > 0)
        {
            char uid_str[256];
            format_client_id_to_string(lease->client_id, lease->client_id_len,
                                      uid_str, sizeof(uid_str));
            fprintf(fp, "\tuid %s;\n", uid_str);
        }

        // Write hostname if present
        if(strlen(lease->client_hostname) > 0)
        {
            fprintf(fp, "\tclient-hostname \"%s\";\n", lease->client_hostname);
        }

        // Write vendor class identifier if present
        if(strlen(lease->vendor_class_identifier) > 0)
        {
            fprintf(fp, "\tvendor-class-identifier \"%s\";\n",
                   lease->vendor_class_identifier);
        }

        // Write abandoned flag if set
        if(lease->is_abandoned)
        {
            fprintf(fp, "\tabandoned;\n");
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
    char time_buf[64];

    inet_ntop(AF_INET, &lease->ip_address, ip_str, INET_ADDRSTRLEN);

    fprintf(fp, "\nlease %s {\n", ip_str);

    // Write timestamps
    format_lease_time(lease->start_time, time_buf, sizeof(time_buf));
    fprintf(fp, "\tstarts %s;\n", time_buf);

    format_lease_time(lease->end_time, time_buf, sizeof(time_buf));
    fprintf(fp, "\tends %s;\n", time_buf);

    if(lease->tstp > 0)
    {
        format_lease_time(lease->tstp, time_buf, sizeof(time_buf));
        fprintf(fp, "\ttstp %s;\n", time_buf);
    }

    if(lease->cltt > 0)
    {
        format_lease_time(lease->cltt, time_buf, sizeof(time_buf));
        fprintf(fp, "\tcltt %s;\n", time_buf);
    }

    // Write binding states
    fprintf(fp, "\tbinding state %s;\n", lease_state_to_string(lease->state));

    if(lease->next_binding_state != LEASE_STATE_FREE)
    {
        fprintf(fp, "\tnext binding state %s;\n", lease_state_to_string(lease->next_binding_state));
    }

    if(lease->rewind_binding_state != LEASE_STATE_FREE)
    {
        fprintf(fp, "\trewind binding state %s;\n", lease_state_to_string(lease->rewind_binding_state));
    }

    // Write hardware address
    fprintf(fp, "\thardware ethernet %02x:%02x:%02x:%02x:%02x:%02x;\n",
            lease->mac_address[0], lease->mac_address[1], lease->mac_address[2],
            lease->mac_address[3], lease->mac_address[4], lease->mac_address[5]);

    // Write client ID if present
    if(lease->client_id_len > 0)
    {
        char uid_str[256];
        format_client_id_to_string(lease->client_id, lease->client_id_len, uid_str, sizeof(uid_str));
        fprintf(fp, "\tuid %s;\n", uid_str);
    }

    // Write hostname if present
    if(strlen(lease->client_hostname) > 0)
    {
        fprintf(fp, "\tclient-hostname \"%s\";\n", lease->client_hostname);
    }

    // Write vendor class identifier if present
    if(strlen(lease->vendor_class_identifier) > 0)
    {
        fprintf(fp, "\tvendor-class-identifier \"%s\";\n", lease->vendor_class_identifier);
    }

    // Write abandoned flag if set
    if(lease->is_abandoned)
    {
        fprintf(fp, "\tabandoned;\n");
    }

    fprintf(fp, "}\n");

    fclose(fp);
    return 0;
}

struct dhcp_lease_t* lease_db_add_lease(struct lease_database_t* db, struct in_addr ip, const uint8_t mac[6], uint32_t lease_time)
{
    if(!db || db->lease_count >= MAX_LEASES) return NULL;

    struct dhcp_lease_t* lease = &db->leases[db->lease_count];
    memset(lease, 0, sizeof(struct dhcp_lease_t));

    time_t now = time(NULL);

    // Basic lease information
    lease->ip_address = ip;
    memcpy(lease->mac_address, mac, 6);

    // Timestamps
    lease->start_time = now;
    lease->end_time = now + lease_time;
    lease->tstp = now;              // Time state was put (initialized now)
    lease->cltt = now;              // Client last transaction time

    // State transitions
    lease->state = LEASE_STATE_ACTIVE;
    lease->next_binding_state = LEASE_STATE_FREE;    // After expiration
    lease->rewind_binding_state = LEASE_STATE_FREE;  // On failure

    // Update string representation
    strncpy(lease->binding_state, "active", sizeof(lease->binding_state) - 1);

    // Flags
    lease->is_abandoned = false;
    lease->is_bootp = false;

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

    time_t now = time(NULL);

    lease->state = LEASE_STATE_RELEASED;
    lease->end_time = now;     // Mark as ended now
    lease->tstp = now;         // State changed
    lease->cltt = now;         // Last transaction

    // Update string representation
    strncpy(lease->binding_state, "released", sizeof(lease->binding_state) - 1);

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
    lease->cltt = now;         // Last transaction

    // If this was a renewal from expired/released state, update tstp
    if(lease->state != LEASE_STATE_ACTIVE)
    {
        lease->tstp = now;
    }

    // Update string representation
    strncpy(lease->binding_state, "active", sizeof(lease->binding_state) - 1);

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
            lease->tstp = now;  // State changed to expired
            strncpy(lease->binding_state, "expired", sizeof(lease->binding_state) - 1);
            expired_count++;
        }
    }

    if(expired_count > 0)
    {
        lease_db_save(db);
    }

    return expired_count;
}

int lease_db_cleanup_expired(struct lease_database_t* db)
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

        if(lease->client_id_len > 0)
        {
            printf("  Client ID: ");
            for(uint32_t j = 0; j < lease->client_id_len; j++)
            {
                printf("%02x", lease->client_id[j]);
            }
            printf("\n");
        }

        if(strlen(lease->vendor_class_identifier) > 0)
        {
            printf("  Vendor: %s\n", lease->vendor_class_identifier);
        }

        if(lease_is_expired(lease))
        {
            printf("  *** EXPIRED ***\n");
        }

        printf("\n");
    }
}


//=============================================================================
// Helper Functions for Extended Lease Information
//=============================================================================

int lease_set_client_id(struct dhcp_lease_t* lease, const uint8_t* client_id, uint32_t len)
{
    if(!lease || !client_id || len > MAX_CLIENT_ID_LEN) return -1;

    memcpy(lease->client_id, client_id, len);
    lease->client_id_len = len;
    return 0;
}

int lease_set_vendor_class(struct dhcp_lease_t* lease, const char* vendor_class)
{
    if(!lease || !vendor_class) return -1;

    strncpy(lease->vendor_class_identifier, vendor_class, MAX_VENDOR_CLASS_LEN - 1);
    lease->vendor_class_identifier[MAX_VENDOR_CLASS_LEN - 1] = '\0';
    return 0;
}

void lease_update_timestamps(struct dhcp_lease_t* lease, time_t now)
{
    if(!lease) return;

    lease->cltt = now;  // Client Last Transaction Time
    // Note: tstp (Time State was Put) should be updated only when state changes
    // Caller should update tstp when changing state
}

void lease_set_state_transition(struct dhcp_lease_t* lease, lease_state_t current, lease_state_t next, lease_state_t rewind)
{
    if(!lease) return;

    time_t now = time(NULL);

    lease->state = current;
    lease->next_binding_state = next;
    lease->rewind_binding_state = rewind;
    lease->tstp = now;  // State changed now

    // Update string representation for file format compatibility
    strncpy(lease->binding_state, lease_state_to_string(current), sizeof(lease->binding_state) - 1);
    lease->binding_state[sizeof(lease->binding_state) - 1] = '\0';
}
