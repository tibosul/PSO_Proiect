#ifndef LEASE_V4_H
#define LEASE_V4_H

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <time.h>

#define MAX_LEASES 1024
#define MAX_CLIENT_HOSTNAME 256
#define MAX_CLIENT_ID_LEN 64
#define MAX_VENDOR_CLASS_LEN 128

typedef enum
{
    LEASE_STATE_FREE = 0,       // Available for allocation
    LEASE_STATE_ACTIVE,         // Currently leased
    LEASE_STATE_EXPIRED,        // Lease time expired
    LEASE_STATE_RELEASED,       // Client released the lease
    LEASE_STATE_ABANDONED,      // Ping check failed or other issue
    LEASE_STATE_RESERVED,       // Static/reserved (from config)
    LEASE_STATE_BACKUP,         // Backup state for failover
    LEASE_STATE_UNKNOWN         // Unknown (used as return value to avoid warnings)
} lease_state_t;

struct dhcp_lease_t
{
    struct in_addr ip_address;
    uint8_t mac_address[6];

    // Timestamps
    time_t start_time;          // Unix timestamp - when lease started (starts)
    time_t end_time;            // Unix timestamp - when lease expires (ends)
    time_t tstp;                // Time State was Put - last state change
    time_t cltt;                // Client Last Transaction Time

    // State management
    lease_state_t state;                    // Current state
    lease_state_t next_binding_state;       // State after expiration
    lease_state_t rewind_binding_state;     // State to revert to on failure

    // Client identification
    uint8_t client_id[MAX_CLIENT_ID_LEN];
    uint32_t client_id_len;
    char client_hostname[MAX_CLIENT_HOSTNAME];

    // Vendor information (Option 60)
    char vendor_class_identifier[MAX_VENDOR_CLASS_LEN];

    // Flags
    bool is_abandoned;          // Marked as abandoned (ping failed)
    bool is_bootp;              // BOOTP client (no expiration)

    // For backward compatibility with file format
    char binding_state[32];     // String representation of state
};

struct lease_database_t
{
    struct dhcp_lease_t leases[MAX_LEASES];
    uint32_t lease_count;
    char filename[256]; // Path to lease file
};

int lease_db_init(struct lease_database_t* db, const char* filename);
void lease_db_free(struct lease_database_t* db);

int lease_db_load(struct lease_database_t* db);
int lease_db_save(struct lease_database_t* db);
int lease_db_append_lease(struct lease_database_t* db, const struct dhcp_lease_t* lease);


struct dhcp_lease_t* lease_db_add_lease(struct lease_database_t* db, struct in_addr ip, const uint8_t mac[6], uint32_t lease_time);
struct dhcp_lease_t* lease_db_find_by_ip(struct lease_database_t* db, struct in_addr ip);
struct dhcp_lease_t* lease_db_find_by_mac(struct lease_database_t* db, const uint8_t mac[6]);
int lease_db_release_lease(struct lease_database_t* db, struct in_addr ip);
int lease_db_renew_lease(struct lease_database_t* db, struct in_addr ip, uint32_t lease_time);

int lease_db_expire_old_leases(struct lease_database_t* db);
int lease_db_cleanup_expired(struct lease_database_t* db);
void lease_db_print(const struct lease_database_t* db);

const char* lease_state_to_string(lease_state_t state);
lease_state_t lease_state_from_string(const char* str);
bool lease_is_expired(const struct dhcp_lease_t* lease);


int lease_set_client_id(struct dhcp_lease_t* lease, const uint8_t* client_id, uint32_t len);
int lease_set_vendor_class(struct dhcp_lease_t* lease, const char* vendor_class);
void lease_update_timestamps(struct dhcp_lease_t* lease, time_t now);
void lease_set_state_transition(struct dhcp_lease_t* lease, lease_state_t current, lease_state_t next, lease_state_t rewind);

// Client ID utilities
int parse_client_id_from_string(const char* str, uint8_t* client_id, uint32_t* len);
void format_client_id_to_string(const uint8_t* client_id, uint32_t len, char* output, size_t output_len);

// Time parsing for ISC DHCP format
time_t parse_lease_time(const char* time_str);
void format_lease_time(time_t timestamp, char* output, size_t output_len);


#endif // LEASE_V4_H