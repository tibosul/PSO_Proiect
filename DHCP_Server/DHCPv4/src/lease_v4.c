#define _GNU_SOURCE
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include "../include/src/lease_v4.h"
#include "../include/utils/string_utils.h"
#include "../include/utils/network_utils.h"
#include "../include/utils/time_utils.h"
#include "../include/utils/encoding_utils.h"

#define MAX_LINE_LEN 1024

const char *lease_state_to_string(lease_state_t state)
{
    switch (state)
    {
    case LEASE_STATE_FREE:
        return "free";
    case LEASE_STATE_ACTIVE:
        return "active";
    case LEASE_STATE_EXPIRED:
        return "expired";
    case LEASE_STATE_RELEASED:
        return "released";
    case LEASE_STATE_ABANDONED:
        return "abandoned";
    case LEASE_STATE_RESERVED:
        return "reserved";
    case LEASE_STATE_BACKUP:
        return "backup";
    default:
        return "unknown";
    }
}

lease_state_t lease_state_from_string(const char *str)
{
    if (strcmp(str, "free") == 0)
        return LEASE_STATE_FREE;
    if (strcmp(str, "active") == 0)
        return LEASE_STATE_ACTIVE;
    if (strcmp(str, "expired") == 0)
        return LEASE_STATE_EXPIRED;
    if (strcmp(str, "released") == 0)
        return LEASE_STATE_RELEASED;
    if (strcmp(str, "abandoned") == 0)
        return LEASE_STATE_ABANDONED;
    if (strcmp(str, "reserved") == 0)
        return LEASE_STATE_RESERVED;
    if (strcmp(str, "backup") == 0)
        return LEASE_STATE_BACKUP;

    return LEASE_STATE_UNKNOWN;
}

bool lease_is_expired(const struct dhcp_lease_t *lease)
{
    time_t now = time(NULL);
    return (lease->state == LEASE_STATE_ACTIVE && lease->end_time < now);
}

int lease_db_init(struct lease_database_t *db, const char *filename)
{
    if (!db || !filename)
        return -1;

    memset(db, 0, sizeof(struct lease_database_t));
    strncpy(db->filename, filename, sizeof(db->filename) - 1);
    db->next_lease_id = 1; // Start at 1 (0 = "no lease")

    // Initialize mutex for thread safety
    if (pthread_mutex_init(&db->db_mutex, NULL) != 0)
    {
        perror("Failed to initialize lease database mutex");
        return -1;
    }
    db->mutex_initialized = true;

    return 0;
}

void lease_db_free(struct lease_database_t *db)
{
    if (db)
    {
        // Destroy mutex if it was initialized
        if (db->mutex_initialized)
        {
            pthread_mutex_destroy(&db->db_mutex);
            db->mutex_initialized = false;
        }
        memset(db, 0, sizeof(struct lease_database_t));
    }
}

// Generate next unique lease ID
uint64_t lease_db_generate_id(struct lease_database_t *db)
{
    if (!db)
        return 0;
    return db->next_lease_id++;
}

// Find lease by ID (stable reference)
struct dhcp_lease_t *lease_db_find_by_id(struct lease_database_t *db, uint64_t lease_id)
{
    if (!db || lease_id == 0)
        return NULL;

    for (uint32_t i = 0; i < db->lease_count; i++)
    {
        if (db->leases[i].lease_id == lease_id)
        {
            return &db->leases[i];
        }
    }
    return NULL;
}

static int parse_lease_block(FILE *fp, struct dhcp_lease_t *lease, char *first_line)
{
    memset(lease, 0, sizeof(struct dhcp_lease_t));

    // Parse "lease x.x.x.x {"
    char *token = strtok(first_line, " \t{");
    if (!token || strcmp(token, "lease") != 0)
        return -1;

    char *ip_str = strtok(NULL, " \t{");
    if (!ip_str || inet_pton(AF_INET, ip_str, &lease->ip_address) != 1)
    {
        return -1;
    }

    // Default state and transitions
    lease->state = LEASE_STATE_FREE;
    lease->next_binding_state = LEASE_STATE_FREE;
    lease->rewind_binding_state = LEASE_STATE_FREE;

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp))
    {
        char *trimmed = trim(line);

        // Skip empty lines and comments
        if (strlen(trimmed) == 0 || trimmed[0] == '#')
            continue;
        if (trimmed[0] == '}')
        {
            return 0;
        }

        char *key = strtok(trimmed, " \t");
        if (!key)
            continue;

        if (strcmp(key, "starts") == 0)
        {
            char *value = strtok(NULL, ";");
            if (value)
            {
                lease->start_time = parse_lease_time(trim(value));
            }
        }
        else if (strcmp(key, "ends") == 0)
        {
            char *value = strtok(NULL, ";");
            if (value)
            {
                lease->end_time = parse_lease_time(trim(value));
            }
        }
        else if (strcmp(key, "tstp") == 0)
        {
            char *value = strtok(NULL, ";");
            if (value)
            {
                lease->tstp = parse_lease_time(trim(value));
            }
        }
        else if (strcmp(key, "cltt") == 0)
        {
            char *value = strtok(NULL, ";");
            if (value)
            {
                lease->cltt = parse_lease_time(trim(value));
            }
        }
        else if (strcmp(key, "hardware") == 0)
        {
            strtok(NULL, " \t"); // Skip hardware type (e.g., "ethernet")
            char *mac_str = strtok(NULL, ";");
            if (mac_str)
            {
                parse_mac_address(trim(mac_str), lease->mac_address);
            }
        }
        else if (strcmp(key, "uid") == 0)
        {
            char *uid_str = strtok(NULL, ";");
            if (uid_str)
            {
                parse_client_id_from_string(trim(uid_str), lease->client_id, &lease->client_id_len);
            }
        }
        else if (strcmp(key, "client-hostname") == 0)
        {
            char *hostname = strtok(NULL, ";");
            if (hostname)
            {
                hostname = trim(hostname);

                // Remove quotes
                hostname = remove_quotes(hostname);
                strncpy(lease->client_hostname, hostname, MAX_CLIENT_HOSTNAME - 1);
            }
        }
        else if (strcmp(key, "vendor-class-identifier") == 0)
        {
            char *vendor = strtok(NULL, ";");
            if (vendor)
            {
                vendor = trim(vendor);

                // Remove quotes
                vendor = remove_quotes(vendor);
                strncpy(lease->vendor_class_identifier, vendor, MAX_VENDOR_CLASS_LEN - 1);
            }
        }
        else if (strcmp(key, "set") == 0)
        {
            // Parse "set lease-id = "123";"
            char *var_name = strtok(NULL, " \t=");
            if (var_name && strcmp(trim(var_name), "lease-id") == 0)
            {
                char *value = strtok(NULL, ";\"");
                if (value)
                {
                    value = trim(value);
                    // Remove quotes if present
                    if (value[0] == '"')
                        value++;
                    lease->lease_id = (uint64_t)strtoull(value, NULL, 10);
                }
            }
        }
        else if (strcmp(key, "binding") == 0)
        {
            char *binding_key = strtok(NULL, " \t");
            if (binding_key && strcmp(binding_key, "state") == 0)
            {
                char *state_str = strtok(NULL, ";");
                if (state_str)
                {
                    state_str = trim(state_str);
                    lease->state = lease_state_from_string(state_str);
                    strncpy(lease->binding_state, state_str, sizeof(lease->binding_state) - 1);
                }
            }
        }
        else if (strcmp(key, "next") == 0)
        {
            char *next_key = strtok(NULL, " \t");
            if (next_key && strcmp(next_key, "binding") == 0)
            {
                char *state_key = strtok(NULL, " \t");
                if (state_key && strcmp(state_key, "state") == 0)
                {
                    char *state_str = strtok(NULL, ";");
                    if (state_str)
                    {
                        lease->next_binding_state = lease_state_from_string(trim(state_str));
                    }
                }
            }
        }
        else if (strcmp(key, "rewind") == 0)
        {
            char *rewind_key = strtok(NULL, " \t");
            if (rewind_key && strcmp(rewind_key, "binding") == 0)
            {
                char *state_key = strtok(NULL, " \t");
                if (state_key && strcmp(state_key, "state") == 0)
                {
                    char *state_str = strtok(NULL, ";");
                    if (state_str)
                    {
                        lease->rewind_binding_state = lease_state_from_string(trim(state_str));
                    }
                }
            }
        }
        else if (strcmp(key, "abandoned;") == 0 || strcmp(key, "abandoned") == 0)
        {
            lease->is_abandoned = true;
        }
        else if (strcmp(key, "set") == 0)
        {
            // Parse vendor-specific options like: set vendor-string = "Cisco";
            // For now, we'll skip these
            continue;
        }
    }
    return -1;
}

int lease_db_load(struct lease_database_t *db)
{
    if (!db)
        return -1;

    FILE *fp = fopen(db->filename, "r");
    if (!fp)
    {
        printf("Lease file %s not found, starting with empty database\n", db->filename);
        db->next_lease_id = 1; // Initialize
        return 0;
    }

    db->lease_count = 0;
    db->next_lease_id = 1; // Will be updated

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp))
    {
        char *trimmed = trim(line);

        // Skip empty lines and comments
        if (strlen(trimmed) == 0 || trimmed[0] == '#')
            continue;

        // Look for lease blocks
        if (strncmp(trimmed, "lease", 5) == 0)
        {
            if (db->lease_count < MAX_LEASES)
            {
                if (parse_lease_block(fp, &db->leases[db->lease_count], trimmed) == 0)
                {
                    // Generate ID if not present in file (backward compatibility)
                    if (db->leases[db->lease_count].lease_id == 0)
                    {
                        db->leases[db->lease_count].lease_id = lease_db_generate_id(db);
                    }
                    else
                    {
                        // Update next_lease_id to be higher than any existing
                        if (db->leases[db->lease_count].lease_id >= db->next_lease_id)
                        {
                            db->next_lease_id = db->leases[db->lease_count].lease_id + 1;
                        }
                    }

                    db->lease_count++;
                }
            }
        }
    }

    fclose(fp);
    printf("Loaded %u leases from %s (next ID: %lu)\n", db->lease_count, db->filename, db->next_lease_id);
    return 0;
}

int lease_db_save(struct lease_database_t *db)
{
    if (!db)
        return -1;

    FILE *fp = fopen(db->filename, "w");
    if (!fp)
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
    for (uint32_t i = 0; i < db->lease_count; i++)
    {
        struct dhcp_lease_t *lease = &db->leases[i];
        char ip_str[INET_ADDRSTRLEN];
        char time_buf[64];

        inet_ntop(AF_INET, &lease->ip_address, ip_str, INET_ADDRSTRLEN);

        fprintf(fp, "\nlease %s {\n", ip_str);

        // Write lease ID as custom field (ISC DHCP compatible)
        if (lease->lease_id > 0)
        {
            fprintf(fp, "\t# Lease ID (custom field)\n");
            fprintf(fp, "\tset lease-id = \"%lu\";\n", lease->lease_id);
        }

        // Write timestamps in ISC DHCP format
        format_lease_time(lease->start_time, time_buf, sizeof(time_buf));
        fprintf(fp, "\tstarts %s;\n", time_buf);

        format_lease_time(lease->end_time, time_buf, sizeof(time_buf));
        fprintf(fp, "\tends %s;\n", time_buf);

        if (lease->tstp > 0)
        {
            format_lease_time(lease->tstp, time_buf, sizeof(time_buf));
            fprintf(fp, "\ttstp %s;\n", time_buf);
        }

        if (lease->cltt > 0)
        {
            format_lease_time(lease->cltt, time_buf, sizeof(time_buf));
            fprintf(fp, "\tcltt %s;\n", time_buf);
        }

        // Write binding states
        fprintf(fp, "\tbinding state %s;\n", lease_state_to_string(lease->state));

        if (lease->next_binding_state != LEASE_STATE_FREE)
        {
            fprintf(fp, "\tnext binding state %s;\n", lease_state_to_string(lease->next_binding_state));
        }

        if (lease->rewind_binding_state != LEASE_STATE_FREE)
        {
            fprintf(fp, "\trewind binding state %s;\n", lease_state_to_string(lease->rewind_binding_state));
        }

        // Write hardware address
        fprintf(fp, "\thardware ethernet %02x:%02x:%02x:%02x:%02x:%02x;\n",
                lease->mac_address[0], lease->mac_address[1], lease->mac_address[2],
                lease->mac_address[3], lease->mac_address[4], lease->mac_address[5]);

        // Write client ID if present
        if (lease->client_id_len > 0)
        {
            char uid_str[256];
            format_client_id_to_string(lease->client_id, lease->client_id_len,
                                       uid_str, sizeof(uid_str));
            fprintf(fp, "\tuid %s;\n", uid_str);
        }

        // Write hostname if present
        if (strlen(lease->client_hostname) > 0)
        {
            fprintf(fp, "\tclient-hostname \"%s\";\n", lease->client_hostname);
        }

        // Write vendor class identifier if present
        if (strlen(lease->vendor_class_identifier) > 0)
        {
            fprintf(fp, "\tvendor-class-identifier \"%s\";\n", lease->vendor_class_identifier);
        }

                // Write abandoned flag if set
        if (lease->is_abandoned)
        {
            fprintf(fp, "\tabandoned;\n");
        }

        fprintf(fp, "}\n");
    }

    fclose(fp);
    return 0;
}

int lease_db_append_lease(struct lease_database_t *db, const struct dhcp_lease_t *lease)
{
    if (!db || !lease)
        return -1;

    FILE *fp = fopen(db->filename, "a");
    if (!fp)
    {
        perror("Failed to open lease file for appending\n");
        return -1;
    }

    char ip_str[INET_ADDRSTRLEN];
    char time_buf[64];

    inet_ntop(AF_INET, &lease->ip_address, ip_str, INET_ADDRSTRLEN);

    fprintf(fp, "\nlease %s {\n", ip_str);

    // Write lease ID as custom field (ISC DHCP compatible)
    if (lease->lease_id > 0)
    {
        fprintf(fp, "\t# Lease ID (custom field)\n");
        fprintf(fp, "\tset lease-id = \"%lu\";\n", lease->lease_id);
    }

    // Write timestamps
    format_lease_time(lease->start_time, time_buf, sizeof(time_buf));
    fprintf(fp, "\tstarts %s;\n", time_buf);

    format_lease_time(lease->end_time, time_buf, sizeof(time_buf));
    fprintf(fp, "\tends %s;\n", time_buf);

    if (lease->tstp > 0)
    {
        format_lease_time(lease->tstp, time_buf, sizeof(time_buf));
        fprintf(fp, "\ttstp %s;\n", time_buf);
    }

    if (lease->cltt > 0)
    {
        format_lease_time(lease->cltt, time_buf, sizeof(time_buf));
        fprintf(fp, "\tcltt %s;\n", time_buf);
    }

    // Write binding states
    fprintf(fp, "\tbinding state %s;\n", lease_state_to_string(lease->state));

    if (lease->next_binding_state != LEASE_STATE_FREE)
    {
        fprintf(fp, "\tnext binding state %s;\n", lease_state_to_string(lease->next_binding_state));
    }

    if (lease->rewind_binding_state != LEASE_STATE_FREE)
    {
        fprintf(fp, "\trewind binding state %s;\n", lease_state_to_string(lease->rewind_binding_state));
    }

    // Write hardware address
    fprintf(fp, "\thardware ethernet %02x:%02x:%02x:%02x:%02x:%02x;\n",
            lease->mac_address[0], lease->mac_address[1], lease->mac_address[2],
            lease->mac_address[3], lease->mac_address[4], lease->mac_address[5]);

    // Write client ID if present
    if (lease->client_id_len > 0)
    {
        char uid_str[256];
        format_client_id_to_string(lease->client_id, lease->client_id_len, uid_str, sizeof(uid_str));
        fprintf(fp, "\tuid %s;\n", uid_str);
    }

    // Write hostname if present
    if (strlen(lease->client_hostname) > 0)
    {
        fprintf(fp, "\tclient-hostname \"%s\";\n", lease->client_hostname);
    }

    // Write vendor class identifier if present
    if (strlen(lease->vendor_class_identifier) > 0)
    {
        fprintf(fp, "\tvendor-class-identifier \"%s\";\n", lease->vendor_class_identifier);
    }

    // Write abandoned flag if set
    if (lease->is_abandoned)
    {
        fprintf(fp, "\tabandoned;\n");
    }

    fprintf(fp, "}\n");

    fclose(fp);
    return 0;
}

struct dhcp_lease_t *lease_db_add_lease(struct lease_database_t *db, struct in_addr ip, const uint8_t mac[6], uint32_t lease_time)
{
    if (!db || db->lease_count >= MAX_LEASES)
        return NULL;

    struct dhcp_lease_t *lease = &db->leases[db->lease_count];
    memset(lease, 0, sizeof(struct dhcp_lease_t));

    time_t now = time(NULL);

    // Generate unique ID
    lease->lease_id = lease_db_generate_id(db);

    // Basic lease information
    lease->ip_address = ip;
    memcpy(lease->mac_address, mac, 6);

    // Timestamps
    lease->start_time = now;
    lease->end_time = now + lease_time;
    lease->tstp = now; // Time state was put (initialized now)
    lease->cltt = now; // Client last transaction time

    // State transitions
    lease->state = LEASE_STATE_ACTIVE;
    lease->next_binding_state = LEASE_STATE_FREE;   // After expiration
    lease->rewind_binding_state = LEASE_STATE_FREE; // On failure

    // Update string representation
    strncpy(lease->binding_state, "active", sizeof(lease->binding_state) - 1);

    // Flags
    lease->is_abandoned = false;
    lease->is_bootp = false;

    db->lease_count++;
    lease_db_append_lease(db, lease);

    return lease;
}

struct dhcp_lease_t *lease_db_find_by_ip(struct lease_database_t *db, struct in_addr ip)
{
    if (!db)
        return NULL;

    for (uint32_t i = 0; i < db->lease_count; i++)
    {
        if (db->leases[i].ip_address.s_addr == ip.s_addr)
        {
            return &db->leases[i];
        }
    }
    return NULL;
}

struct dhcp_lease_t *lease_db_find_by_mac(struct lease_database_t *db, const uint8_t mac[6])
{
    if (!db)
        return NULL;

    for (uint32_t i = 0; i < db->lease_count; i++)
    {
        if (memcmp(db->leases[i].mac_address, mac, 6) == 0)
        {
            return &db->leases[i];
        }
    }
    return NULL;
}

int lease_db_release_lease(struct lease_database_t *db, struct in_addr ip)
{
    struct dhcp_lease_t *lease = lease_db_find_by_ip(db, ip);
    if (!lease)
        return -1;

    time_t now = time(NULL);

    lease->state = LEASE_STATE_RELEASED;
    lease->end_time = now; // Mark as ended now
    lease->tstp = now;     // State changed
    lease->cltt = now;     // Last transaction

    // Update string representation
    strncpy(lease->binding_state, "released", sizeof(lease->binding_state) - 1);

    lease_db_save(db);
    return 0;
}

int lease_db_renew_lease(struct lease_database_t *db, struct in_addr ip, uint32_t lease_time)
{
    struct dhcp_lease_t *lease = lease_db_find_by_ip(db, ip);
    if (!lease)
        return -1;

    time_t now = time(NULL);
    lease->start_time = now;
    lease->end_time = now + lease_time;
    lease->state = LEASE_STATE_ACTIVE;
    lease->cltt = now; // Last transaction

    // If this was a renewal from expired/released state, update tstp
    if (lease->state != LEASE_STATE_ACTIVE)
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
    if (!db)
        return -1;

    uint32_t expired_count = 0;
    time_t now = time(NULL);

    for (uint32_t i = 0; i < db->lease_count; i++)
    {
        struct dhcp_lease_t *lease = &db->leases[i];
        if (lease->state == LEASE_STATE_ACTIVE && lease->end_time < now)
        {
            lease->state = LEASE_STATE_EXPIRED;
            lease->tstp = now; // State changed to expired
            strncpy(lease->binding_state, "expired", sizeof(lease->binding_state) - 1);
            expired_count++;
        }
    }

    if (expired_count > 0)
    {
        lease_db_save(db);
    }

    return expired_count;
}

int lease_db_cleanup_expired(struct lease_database_t *db)
{
    if (!db)
        return -1;

    uint32_t removed = 0;

    uint32_t i = 0;
    while (i < db->lease_count)
    {
        struct dhcp_lease_t *lease = &db->leases[i];

        if (lease->state == LEASE_STATE_EXPIRED || lease->state == LEASE_STATE_RELEASED)
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

    if (removed > 0)
    {
        lease_db_save(db);
    }

    return removed;
}

void lease_db_print(const struct lease_database_t *db)
{
    if (!db)
        return;

    printf("--- Lease Database ---\n");
    printf("File: %s\n", db->filename);
    printf("Total Leases: %u\n\n", db->lease_count);

    for (uint32_t i = 0; i < db->lease_count; i++)
    {
        const struct dhcp_lease_t *lease = &db->leases[i];
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &lease->ip_address, ip_str, INET_ADDRSTRLEN);

        printf("Lease %u:\n", i + 1);
        printf("  IP: %s\n", ip_str);
        printf("  MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
               lease->mac_address[0], lease->mac_address[1], lease->mac_address[2],
               lease->mac_address[3], lease->mac_address[4], lease->mac_address[5]);
        printf("  State: %s\n", lease_state_to_string(lease->state));
        printf("  Start: %s", ctime(&lease->start_time));
        printf("  End: %s", ctime(&lease->end_time));

        if (strlen(lease->client_hostname) > 0)
        {
            printf("  Hostname: %s\n", lease->client_hostname);
        }

        if (lease->client_id_len > 0)
        {
            printf("  Client ID: ");
            for (uint32_t j = 0; j < lease->client_id_len; j++)
            {
                printf("%02x", lease->client_id[j]);
            }
            printf("\n");
        }

        if (strlen(lease->vendor_class_identifier) > 0)
        {
            printf("  Vendor: %s\n", lease->vendor_class_identifier);
        }

        if (lease_is_expired(lease))
        {
            printf("  *** EXPIRED ***\n");
        }

        printf("\n");
    }
}

//=============================================================================
// Helper Functions for Extended Lease Information
//=============================================================================

int lease_set_client_id(struct dhcp_lease_t *lease, const uint8_t *client_id, uint32_t len)
{
    if (!lease || !client_id || len > MAX_CLIENT_ID_LEN)
        return -1;

    memcpy(lease->client_id, client_id, len);
    lease->client_id_len = len;
    return 0;
}

int lease_set_vendor_class(struct dhcp_lease_t *lease, const char *vendor_class)
{
    if (!lease || !vendor_class)
        return -1;

    strncpy(lease->vendor_class_identifier, vendor_class, MAX_VENDOR_CLASS_LEN - 1);
    lease->vendor_class_identifier[MAX_VENDOR_CLASS_LEN - 1] = '\0';
    return 0;
}

void lease_update_timestamps(struct dhcp_lease_t *lease, time_t now)
{
    if (!lease)
        return;

    lease->cltt = now; // Client Last Transaction Time
    // Note: tstp (Time State was Put) should be updated only when state changes
    // Caller should update tstp when changing state
}

void lease_set_state_transition(struct dhcp_lease_t *lease, lease_state_t current, lease_state_t next, lease_state_t rewind)
{
    if (!lease)
        return;

    time_t now = time(NULL);

    lease->state = current;
    lease->next_binding_state = next;
    lease->rewind_binding_state = rewind;
    lease->tstp = now; // State changed now

    // Update string representation for file format compatibility
    strncpy(lease->binding_state, lease_state_to_string(current), sizeof(lease->binding_state) - 1);
    lease->binding_state[sizeof(lease->binding_state) - 1] = '\0';
}

//=============================================================================
// Thread-Safe Operations (Paso 1: Mutex Protection)
//=============================================================================

void lease_db_lock(struct lease_database_t *db)
{
    if (db && db->mutex_initialized)
    {
        pthread_mutex_lock(&db->db_mutex);
    }
}

void lease_db_unlock(struct lease_database_t *db)
{
    if (db && db->mutex_initialized)
    {
        pthread_mutex_unlock(&db->db_mutex);
    }
}

struct dhcp_lease_t *lease_db_add_lease_safe(struct lease_database_t *db, struct in_addr ip, const uint8_t mac[6], uint32_t lease_time)
{
    if (!db)
        return NULL;

    lease_db_lock(db);
    struct dhcp_lease_t *result = lease_db_add_lease(db, ip, mac, lease_time);
    lease_db_unlock(db);

    return result;
}

int lease_db_find_by_ip_safe(struct lease_database_t *db, struct in_addr ip, struct dhcp_lease_t *out_lease)
{
    if (!db || !out_lease)
        return -1;

    lease_db_lock(db);

    struct dhcp_lease_t *lease = lease_db_find_by_ip(db, ip);
    if (lease)
    {
        memcpy(out_lease, lease, sizeof(struct dhcp_lease_t));
        lease_db_unlock(db);
        return 0;
    }

    lease_db_unlock(db);
    return -1;
}

int lease_db_find_by_mac_safe(struct lease_database_t *db, const uint8_t mac[6], struct dhcp_lease_t *out_lease)
{
    if (!db || !mac || !out_lease)
        return -1;

    lease_db_lock(db);

    struct dhcp_lease_t *lease = lease_db_find_by_mac(db, mac);
    if (lease)
    {
        memcpy(out_lease, lease, sizeof(struct dhcp_lease_t));
        lease_db_unlock(db);
        return 0;
    }

    lease_db_unlock(db);
    return -1;
}

int lease_db_find_by_id_safe(struct lease_database_t *db, uint64_t lease_id, struct dhcp_lease_t *out_lease)
{
    if (!db || !out_lease)
        return -1;

    lease_db_lock(db);

    struct dhcp_lease_t *lease = lease_db_find_by_id(db, lease_id);
    if (lease)
    {
        memcpy(out_lease, lease, sizeof(struct dhcp_lease_t));
        lease_db_unlock(db);
        return 0;
    }

    lease_db_unlock(db);
    return -1;
}

int lease_db_release_lease_safe(struct lease_database_t *db, struct in_addr ip)
{
    if (!db)
        return -1;

    lease_db_lock(db);
    int result = lease_db_release_lease(db, ip);
    lease_db_unlock(db);

    return result;
}

int lease_db_renew_lease_safe(struct lease_database_t *db, struct in_addr ip, uint32_t lease_time)
{
    if (!db)
        return -1;

    lease_db_lock(db);
    int result = lease_db_renew_lease(db, ip, lease_time);
    lease_db_unlock(db);

    return result;
}

int lease_db_expire_old_leases_safe(struct lease_database_t *db)
{
    if (!db)
        return -1;

    lease_db_lock(db);
    int result = lease_db_expire_old_leases(db);
    lease_db_unlock(db);

    return result;
}

int lease_db_cleanup_expired_safe(struct lease_database_t *db)
{
    if (!db)
        return -1;

    lease_db_lock(db);
    int result = lease_db_cleanup_expired(db);
    lease_db_unlock(db);

    return result;
}

int lease_db_save_safe(struct lease_database_t *db)
{
    if (!db)
        return -1;

    lease_db_lock(db);
    int result = lease_db_save(db);
    lease_db_unlock(db);

    return result;
}

//=============================================================================
// Timer Thread Operations (Paso 2: Automatic Expiration)
//=============================================================================

// Forward declaration of thread function
static void *lease_timer_thread_func(void *arg);

int lease_timer_init(struct lease_timer_t *timer, struct lease_database_t *db, uint32_t check_interval_sec)
{
    if (!timer || !db || check_interval_sec == 0)
        return -1;

    memset(timer, 0, sizeof(struct lease_timer_t));

    timer->db = db;
    timer->check_interval_sec = check_interval_sec;
    timer->running = false;

    // Initialize mutex and condition variable
    if (pthread_mutex_init(&timer->timer_mutex, NULL) != 0)
    {
        perror("Failed to initialize timer mutex");
        return -1;
    }

    if (pthread_cond_init(&timer->timer_cond, NULL) != 0)
    {
        perror("Failed to initialize timer condition variable");
        pthread_mutex_destroy(&timer->timer_mutex);
        return -1;
    }

    timer->mutex_initialized = true;

    printf("Lease timer initialized (check interval: %u seconds)\n", check_interval_sec);
    return 0;
}

int lease_timer_start(struct lease_timer_t *timer)
{
    if (!timer || !timer->mutex_initialized)
        return -1;

    pthread_mutex_lock(&timer->timer_mutex);

    // Check if already running
    if (timer->running)
    {
        pthread_mutex_unlock(&timer->timer_mutex);
        return -1; // Already running
    }

    timer->running = true;
    pthread_mutex_unlock(&timer->timer_mutex);

    // Create the timer thread
    if (pthread_create(&timer->timer_thread, NULL, lease_timer_thread_func, timer) != 0)
    {
        perror("Failed to create timer thread");
        pthread_mutex_lock(&timer->timer_mutex);
        timer->running = false;
        pthread_mutex_unlock(&timer->timer_mutex);
        return -1;
    }

    printf("Lease timer thread started\n");
    return 0;
}

void lease_timer_stop(struct lease_timer_t *timer)
{
    if (!timer || !timer->mutex_initialized)
        return;

    // Signal thread to stop
    pthread_mutex_lock(&timer->timer_mutex);
    if (!timer->running)
    {
        pthread_mutex_unlock(&timer->timer_mutex);
        return; // Already stopped
    }

    timer->running = false;
    pthread_cond_signal(&timer->timer_cond); // Wake up thread
    pthread_mutex_unlock(&timer->timer_mutex);

    // Wait for thread to terminate
    pthread_join(timer->timer_thread, NULL);

    // Clean up synchronization primitives
    pthread_mutex_destroy(&timer->timer_mutex);
    pthread_cond_destroy(&timer->timer_cond);
    timer->mutex_initialized = false;

    printf("Lease timer thread stopped\n");
}

void lease_timer_wakeup(struct lease_timer_t *timer)
{
    if (!timer || !timer->mutex_initialized)
        return;

    pthread_mutex_lock(&timer->timer_mutex);
    pthread_cond_signal(&timer->timer_cond); // Wake up thread immediately
    pthread_mutex_unlock(&timer->timer_mutex);
}

bool lease_timer_is_running(const struct lease_timer_t *timer)
{
    if (!timer || !timer->mutex_initialized)
        return false;

    // Note: This is a simple check without locking
    // For stricter thread safety, you could add a mutex lock here
    return timer->running;
}

// Timer thread function - runs in background
static void *lease_timer_thread_func(void *arg)
{
    struct lease_timer_t *timer = (struct lease_timer_t *)arg;
    if (!timer)
        return NULL;

    printf("Timer thread started (PID: %d, TID: %lu)\n", getpid(), pthread_self());

    while (1)
    {
        // Calculate absolute time for timed wait
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timer->check_interval_sec;

        // Wait with timeout (can be woken up early)
        pthread_mutex_lock(&timer->timer_mutex);
        pthread_cond_timedwait(&timer->timer_cond, &timer->timer_mutex, &ts);

        // Check if we should stop
        bool should_run = timer->running;
        pthread_mutex_unlock(&timer->timer_mutex);

        if (!should_run)
        {
            break; // Exit loop
        }

        // Perform lease expiration check (thread-safe)
        int expired = lease_db_expire_old_leases_safe(timer->db);

        if (expired > 0)
        {
            printf("[Timer] Auto-expired %d leases\n", expired);

            // Optional: Save to disk after expiration
            // lease_db_save_safe(timer->db);
        }
    }

    printf("Timer thread exiting\n");
    return NULL;
}

//=============================================================================
// I/O Queue Operations (Paso 3: Async Disk I/O)
//=============================================================================

// Forward declaration of I/O thread function
static void *lease_io_thread_func(void *arg);

int lease_io_init(struct lease_io_queue_t *io_queue, struct lease_database_t *db)
{
    if (!io_queue || !db)
        return -1;

    memset(io_queue, 0, sizeof(struct lease_io_queue_t));

    io_queue->db = db;
    io_queue->running = false;
    io_queue->head = 0;
    io_queue->tail = 0;
    io_queue->count = 0;
    io_queue->operations_processed = 0;
    io_queue->operations_dropped = 0;

    // Initialize mutex and condition variable
    if (pthread_mutex_init(&io_queue->queue_mutex, NULL) != 0)
    {
        perror("Failed to initialize I/O queue mutex");
        return -1;
    }

    if (pthread_cond_init(&io_queue->queue_cond, NULL) != 0)
    {
        perror("Failed to initialize I/O queue condition variable");
        pthread_mutex_destroy(&io_queue->queue_mutex);
        return -1;
    }

    io_queue->mutex_initialized = true;

    printf("I/O queue initialized (buffer size: %d)\n", IO_QUEUE_SIZE);
    return 0;
}

int lease_io_start(struct lease_io_queue_t *io_queue)
{
    if (!io_queue || !io_queue->mutex_initialized)
        return -1;

    pthread_mutex_lock(&io_queue->queue_mutex);

    // Check if already running
    if (io_queue->running)
    {
        pthread_mutex_unlock(&io_queue->queue_mutex);
        return -1; // Already running
    }

    io_queue->running = true;
    pthread_mutex_unlock(&io_queue->queue_mutex);

    // Create the I/O thread
    if (pthread_create(&io_queue->io_thread, NULL, lease_io_thread_func, io_queue) != 0)
    {
        perror("Failed to create I/O thread");
        pthread_mutex_lock(&io_queue->queue_mutex);
        io_queue->running = false;
        pthread_mutex_unlock(&io_queue->queue_mutex);
        return -1;
    }

    printf("I/O thread started\n");
    return 0;
}

void lease_io_stop(struct lease_io_queue_t *io_queue)
{
    if (!io_queue || !io_queue->mutex_initialized)
        return;

    // Queue a shutdown operation
    struct io_operation_t shutdown_op;
    shutdown_op.type = IO_OP_SHUTDOWN;
    shutdown_op.timestamp = time(NULL);

    pthread_mutex_lock(&io_queue->queue_mutex);

    if (!io_queue->running)
    {
        pthread_mutex_unlock(&io_queue->queue_mutex);
        return; // Already stopped
    }

    // Add shutdown operation to queue
    if (io_queue->count < IO_QUEUE_SIZE)
    {
        io_queue->queue[io_queue->tail] = shutdown_op;
        io_queue->tail = (io_queue->tail + 1) % IO_QUEUE_SIZE;
        io_queue->count++;
    }

    pthread_cond_signal(&io_queue->queue_cond); // Wake up thread
    pthread_mutex_unlock(&io_queue->queue_mutex);

    // Wait for thread to terminate
    pthread_join(io_queue->io_thread, NULL);

    // Clean up synchronization primitives
    pthread_mutex_destroy(&io_queue->queue_mutex);
    pthread_cond_destroy(&io_queue->queue_cond);
    io_queue->mutex_initialized = false;

    printf("I/O thread stopped (processed: %lu, dropped: %lu)\n",
           io_queue->operations_processed, io_queue->operations_dropped);
}

int lease_io_queue_save_lease(struct lease_io_queue_t *io_queue, const struct dhcp_lease_t *lease)
{
    if (!io_queue || !lease || !io_queue->mutex_initialized)
        return -1;

    pthread_mutex_lock(&io_queue->queue_mutex);

    // Check if queue is full
    if (io_queue->count >= IO_QUEUE_SIZE)
    {
        io_queue->operations_dropped++;
        pthread_mutex_unlock(&io_queue->queue_mutex);
        fprintf(stderr, "[I/O Queue] Queue full, operation dropped\n");
        return -1;
    }

    // Add operation to queue
    struct io_operation_t op;
    op.type = IO_OP_SAVE_LEASE;
    op.lease = *lease; // Copy lease data
    op.timestamp = time(NULL);

    io_queue->queue[io_queue->tail] = op;
    io_queue->tail = (io_queue->tail + 1) % IO_QUEUE_SIZE;
    io_queue->count++;

    pthread_cond_signal(&io_queue->queue_cond); // Wake up I/O thread
    pthread_mutex_unlock(&io_queue->queue_mutex);

    return 0;
}

int lease_io_queue_save_all(struct lease_io_queue_t *io_queue)
{
    if (!io_queue || !io_queue->mutex_initialized)
        return -1;

    pthread_mutex_lock(&io_queue->queue_mutex);

    // Check if queue is full
    if (io_queue->count >= IO_QUEUE_SIZE)
    {
        io_queue->operations_dropped++;
        pthread_mutex_unlock(&io_queue->queue_mutex);
        fprintf(stderr, "[I/O Queue] Queue full, save all operation dropped\n");
        return -1;
    }

    // Add operation to queue
    struct io_operation_t op;
    op.type = IO_OP_SAVE_ALL;
    op.timestamp = time(NULL);

    io_queue->queue[io_queue->tail] = op;
    io_queue->tail = (io_queue->tail + 1) % IO_QUEUE_SIZE;
    io_queue->count++;

    pthread_cond_signal(&io_queue->queue_cond); // Wake up I/O thread
    pthread_mutex_unlock(&io_queue->queue_mutex);

    return 0;
}

void lease_io_get_stats(struct lease_io_queue_t *io_queue, uint64_t *processed, uint64_t *dropped, uint32_t *pending)
{
    if (!io_queue || !io_queue->mutex_initialized)
        return;

    pthread_mutex_lock(&io_queue->queue_mutex);

    if (processed)
        *processed = io_queue->operations_processed;
    if (dropped)
        *dropped = io_queue->operations_dropped;
    if (pending)
        *pending = io_queue->count;

    pthread_mutex_unlock(&io_queue->queue_mutex);
}

bool lease_io_is_running(const struct lease_io_queue_t *io_queue)
{
    if (!io_queue || !io_queue->mutex_initialized)
        return false;

    return io_queue->running;
}

// I/O thread function - processes operations from queue
static void *lease_io_thread_func(void *arg)
{
    struct lease_io_queue_t *io_queue = (struct lease_io_queue_t *)arg;
    if (!io_queue)
        return NULL;

    printf("I/O thread started (PID: %d, TID: %lu)\n", getpid(), pthread_self());

    while (1)
    {
        pthread_mutex_lock(&io_queue->queue_mutex);

        // Wait for operations in queue
        while (io_queue->count == 0 && io_queue->running)
        {
            pthread_cond_wait(&io_queue->queue_cond, &io_queue->queue_mutex);
        }

        // Check if we should exit (shutdown operation or stopped)
        if (io_queue->count == 0 && !io_queue->running)
        {
            pthread_mutex_unlock(&io_queue->queue_mutex);
            break;
        }

        // Dequeue operation
        struct io_operation_t op = io_queue->queue[io_queue->head];
        io_queue->head = (io_queue->head + 1) % IO_QUEUE_SIZE;
        io_queue->count--;

        pthread_mutex_unlock(&io_queue->queue_mutex);

        // Process operation (outside lock to avoid blocking producers)
        switch (op.type)
        {
        case IO_OP_SAVE_LEASE:
            // Append single lease to file
            if (lease_db_append_lease(io_queue->db, &op.lease) == 0)
            {
                printf("[I/O] Saved lease: %s\n", inet_ntoa(op.lease.ip_address));
            }
            else
            {
                fprintf(stderr, "[I/O] Failed to save lease\n");
            }
            break;

        case IO_OP_SAVE_ALL:
            // Save entire database
            if (lease_db_save_safe(io_queue->db) == 0)
            {
                printf("[I/O] Saved full database\n");
            }
            else
            {
                fprintf(stderr, "[I/O] Failed to save database\n");
            }
            break;

        case IO_OP_SHUTDOWN:
            printf("[I/O] Shutdown signal received\n");
            pthread_mutex_lock(&io_queue->queue_mutex);
            io_queue->running = false;
            pthread_mutex_unlock(&io_queue->queue_mutex);
            goto exit_loop; // Exit cleanly

        default:
            fprintf(stderr, "[I/O] Unknown operation type: %d\n", op.type);
            break;
        }

        // Update statistics
        pthread_mutex_lock(&io_queue->queue_mutex);
        io_queue->operations_processed++;
        pthread_mutex_unlock(&io_queue->queue_mutex);
    }

exit_loop:
    printf("I/O thread exiting\n");
    return NULL;
}

//=============================================================================
// Unified Server Structure (Paso 4: Signal Handling & Server Management)
//=============================================================================

// Global pointer for signal handler (only way to pass context to signal handlers)
static struct dhcp_server_t *g_server_instance = NULL;

// Signal handler function
static void dhcp_server_signal_handler(int signum)
{
    if (!g_server_instance)
        return;

    switch (signum)
    {
    case SIGINT:
    case SIGTERM:
        printf("\n[Signal] Received %s, initiating shutdown...\n",
               signum == SIGINT ? "SIGINT" : "SIGTERM");
        g_server_instance->shutdown_requested = 1;

        // Wake up wait_for_shutdown if it's blocking
        if (g_server_instance->mutex_initialized)
        {
            pthread_mutex_lock(&g_server_instance->server_mutex);
            pthread_cond_signal(&g_server_instance->server_cond);
            pthread_mutex_unlock(&g_server_instance->server_mutex);
        }
        break;

    case SIGHUP:
        printf("\n[Signal] Received SIGHUP, reload requested\n");
        g_server_instance->reload_requested = 1;
        break;

    default:
        break;
    }
}

int dhcp_server_init(struct dhcp_server_t *server, const char *lease_file, uint32_t timer_interval, bool enable_async_io)
{
    if (!server || !lease_file)
        return -1;

    memset(server, 0, sizeof(struct dhcp_server_t));

    // Allocate and initialize lease database
    server->lease_db = malloc(sizeof(struct lease_database_t));
    if (!server->lease_db)
    {
        perror("Failed to allocate lease database");
        return -1;
    }

    if (lease_db_init(server->lease_db, lease_file) != 0)
    {
        free(server->lease_db);
        return -1;
    }

    // Load existing leases
    lease_db_load(server->lease_db);

    // Initialize timer if interval > 0
    if (timer_interval > 0)
    {
        server->timer = malloc(sizeof(struct lease_timer_t));
        if (!server->timer)
        {
            perror("Failed to allocate timer");
            lease_db_free(server->lease_db);
            free(server->lease_db);
            return -1;
        }

        if (lease_timer_init(server->timer, server->lease_db, timer_interval) != 0)
        {
            free(server->timer);
            server->timer = NULL;
            lease_db_free(server->lease_db);
            free(server->lease_db);
            return -1;
        }
    }

    // Initialize I/O queue if enabled
    if (enable_async_io)
    {
        server->io_queue = malloc(sizeof(struct lease_io_queue_t));
        if (!server->io_queue)
        {
            perror("Failed to allocate I/O queue");
            if (server->timer)
            {
                free(server->timer);
            }
            lease_db_free(server->lease_db);
            free(server->lease_db);
            return -1;
        }

        if (lease_io_init(server->io_queue, server->lease_db) != 0)
        {
            free(server->io_queue);
            server->io_queue = NULL;
            if (server->timer)
            {
                free(server->timer);
            }
            lease_db_free(server->lease_db);
            free(server->lease_db);
            return -1;
        }
    }

    // Initialize synchronization
    if (pthread_mutex_init(&server->server_mutex, NULL) != 0)
    {
        perror("Failed to initialize server mutex");
        if (server->io_queue)
            free(server->io_queue);
        if (server->timer)
            free(server->timer);
        lease_db_free(server->lease_db);
        free(server->lease_db);
        return -1;
    }

    if (pthread_cond_init(&server->server_cond, NULL) != 0)
    {
        perror("Failed to initialize server condition variable");
        pthread_mutex_destroy(&server->server_mutex);
        if (server->io_queue)
            free(server->io_queue);
        if (server->timer)
            free(server->timer);
        lease_db_free(server->lease_db);
        free(server->lease_db);
        return -1;
    }

    server->mutex_initialized = true;
    server->shutdown_requested = 0;
    server->reload_requested = 0;

    printf("✓ DHCP server initialized\n");
    return 0;
}

int dhcp_server_start(struct dhcp_server_t *server)
{
    if (!server)
        return -1;

    // Start timer thread
    if (server->timer)
    {
        if (lease_timer_start(server->timer) != 0)
        {
            fprintf(stderr, "Failed to start timer thread\n");
            return -1;
        }
    }

    // Start I/O queue thread
    if (server->io_queue)
    {
        if (lease_io_start(server->io_queue) != 0)
        {
            fprintf(stderr, "Failed to start I/O thread\n");
            if (server->timer)
                lease_timer_stop(server->timer);
            return -1;
        }
    }

    printf("✓ DHCP server started\n");
    return 0;
}

void dhcp_server_stop(struct dhcp_server_t *server)
{
    if (!server)
        return;

    printf("Stopping DHCP server...\n");

    // Stop timer thread
    if (server->timer)
    {
        lease_timer_stop(server->timer);
        free(server->timer);
        server->timer = NULL;
    }

    // Stop I/O queue (flushes pending operations)
    if (server->io_queue)
    {
        lease_io_stop(server->io_queue);
        free(server->io_queue);
        server->io_queue = NULL;
    }

    // Save database one final time
    if (server->lease_db)
    {
        printf("Saving lease database...\n");
        lease_db_save(server->lease_db);
        lease_db_free(server->lease_db);
        free(server->lease_db);
        server->lease_db = NULL;
    }

    // Clean up synchronization
    if (server->mutex_initialized)
    {
        pthread_mutex_destroy(&server->server_mutex);
        pthread_cond_destroy(&server->server_cond);
        server->mutex_initialized = false;
    }

    // Clear global instance
    if (g_server_instance == server)
    {
        g_server_instance = NULL;
    }

    printf("✓ DHCP server stopped\n");
}

void dhcp_server_setup_signals(struct dhcp_server_t *server)
{
    if (!server)
        return;

    // Set global instance for signal handler
    g_server_instance = server;

    // Install signal handlers
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = dhcp_server_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    printf("✓ Signal handlers installed (SIGINT, SIGTERM, SIGHUP)\n");
}

void dhcp_server_wait_for_shutdown(struct dhcp_server_t *server)
{
    if (!server || !server->mutex_initialized)
        return;

    pthread_mutex_lock(&server->server_mutex);

    while (!server->shutdown_requested)
    {
        pthread_cond_wait(&server->server_cond, &server->server_mutex);
    }

    pthread_mutex_unlock(&server->server_mutex);
}

bool dhcp_server_check_reload(struct dhcp_server_t *server)
{
    if (!server)
        return false;

    bool reload = server->reload_requested;
    if (reload)
    {
        server->reload_requested = 0; // Clear flag
    }
    return reload;
}

void dhcp_server_print_stats(const struct dhcp_server_t *server)
{
    if (!server)
        return;

    printf("\n=== DHCP Server Statistics ===\n");

    // Lease database stats
    if (server->lease_db)
    {
        printf("Lease Database:\n");
        printf("  Total leases: %u\n", server->lease_db->lease_count);
        printf("  Next lease ID: %lu\n", server->lease_db->next_lease_id);
    }

    // Timer stats
    if (server->timer)
    {
        printf("Timer Thread:\n");
        printf("  Status: %s\n", lease_timer_is_running(server->timer) ? "Running" : "Stopped");
        printf("  Check interval: %u seconds\n", server->timer->check_interval_sec);
    }

    // I/O queue stats
    if (server->io_queue)
    {
        uint64_t processed, dropped;
        uint32_t pending;
        lease_io_get_stats(server->io_queue, &processed, &dropped, &pending);

        printf("I/O Queue:\n");
        printf("  Status: %s\n", lease_io_is_running(server->io_queue) ? "Running" : "Stopped");
        printf("  Operations processed: %lu\n", processed);
        printf("  Operations dropped: %lu\n", dropped);
        printf("  Operations pending: %u\n", pending);
    }

    printf("==============================\n\n");
}
