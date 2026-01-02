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
    LEASE_STATE_FREE = 0,  // Available for allocation
    LEASE_STATE_ACTIVE,    // Currently leased
    LEASE_STATE_EXPIRED,   // Lease time expired
    LEASE_STATE_RELEASED,  // Client released the lease
    LEASE_STATE_ABANDONED, // Ping check failed or other issue
    LEASE_STATE_RESERVED,  // Static/reserved (from config)
    LEASE_STATE_BACKUP,    // Backup state for failover
    LEASE_STATE_UNKNOWN    // Unknown (used as return value to avoid warnings)
} lease_state_t;

struct dhcp_lease_t
{
    uint64_t lease_id; // Unique lease identifier (never changes)
    struct in_addr ip_address;
    uint8_t mac_address[6];

    // Timestamps
    time_t start_time; // Unix timestamp - when lease started (starts)
    time_t end_time;   // Unix timestamp - when lease expires (ends)
    time_t tstp;       // Time State was Put - last state change
    time_t cltt;       // Client Last Transaction Time

    // State management
    lease_state_t state;                // Current state
    lease_state_t next_binding_state;   // State after expiration
    lease_state_t rewind_binding_state; // State to revert to on failure

    // Client identification
    uint8_t client_id[MAX_CLIENT_ID_LEN];
    uint32_t client_id_len;
    char client_hostname[MAX_CLIENT_HOSTNAME];

    // Vendor information (Option 60)
    char vendor_class_identifier[MAX_VENDOR_CLASS_LEN];

    // Flags
    bool is_abandoned; // Marked as abandoned (ping failed)
    bool is_bootp;     // BOOTP client (no expiration)

    // For backward compatibility with file format
    char binding_state[32]; // String representation of state
};

struct lease_database_t
{
    struct dhcp_lease_t leases[MAX_LEASES];
    uint32_t lease_count;
    char filename[256];     // Path to lease file
    uint64_t next_lease_id; // Counter for generating unique IDs
};

// ----------------------------------------------------------------------------------------------

/**
 * @brief Initialize a lease database.
 * @param db Pointer to the lease database structure.
 * @param filename Path to the lease file.
 * @return 0 on success, -1 on failure.
 */
int lease_db_init(struct lease_database_t *db, const char *filename);

/**
 * @brief Free resources associated with the lease database.
 * @param db Pointer to the lease database structure.
 */
void lease_db_free(struct lease_database_t *db);


/**
 * @brief Load leases from the lease file into the database.
 * @param db Pointer to the lease database structure.
 * @return 0 on success, -1 on failure.
 *
 * Reads the ISC DHCP compatible lease file and populates the lease database.
 * If the file doesn't exist, initializes an empty database.
 * Automatically generates lease IDs for backward compatibility.
 */
int lease_db_load(struct lease_database_t *db);

/**
 * @brief Save all leases to the lease file.
 * @param db Pointer to the lease database structure.
 * @return 0 on success, -1 on failure.
 *
 * Writes the entire lease database to disk in ISC DHCP format.
 * This is a full rewrite of the file (not append).
 */
int lease_db_save(struct lease_database_t *db);

/**
 * @brief Append a single lease to the lease file.
 * @param db Pointer to the lease database structure.
 * @param lease Pointer to the lease to append.
 * @return 0 on success, -1 on failure.
 *
 * Appends a lease entry to the end of the lease file without rewriting
 * the entire file. More efficient for real-time lease updates.
 */
int lease_db_append_lease(struct lease_database_t *db, const struct dhcp_lease_t *lease);

/**
 * @brief Generate next unique lease ID.
 * @param db Pointer to the lease database structure.
 * @return A unique 64-bit lease ID, or 0 on failure.
 *
 * Generates monotonically increasing lease IDs. These IDs are stable
 * and never change for a lease, even if the lease is renewed.
 */
uint64_t lease_db_generate_id(struct lease_database_t *db);

/**
 * @brief Find a lease by its unique ID.
 * @param db Pointer to the lease database structure.
 * @param lease_id The unique lease identifier to search for.
 * @return Pointer to the lease if found, NULL otherwise.
 *
 * Provides stable reference to a lease using its immutable ID.
 * Lease IDs never change, unlike IP addresses or MAC addresses.
 */
struct dhcp_lease_t *lease_db_find_by_id(struct lease_database_t *db, uint64_t lease_id);

/**
 * @brief Add a new lease to the database.
 * @param db Pointer to the lease database structure.
 * @param ip IP address to lease.
 * @param mac Client MAC address (6 bytes).
 * @param lease_time Lease duration in seconds.
 * @return Pointer to the newly created lease, or NULL on failure.
 *
 * Creates a new lease with ACTIVE state. Automatically sets start_time
 * to now and calculates end_time based on lease_time.
 */
struct dhcp_lease_t *lease_db_add_lease(struct lease_database_t *db, struct in_addr ip, const uint8_t mac[6], uint32_t lease_time);

/**
 * @brief Find a lease by IP address.
 * @param db Pointer to the lease database structure.
 * @param ip IP address to search for.
 * @return Pointer to the lease if found, NULL otherwise.
 */
struct dhcp_lease_t *lease_db_find_by_ip(struct lease_database_t *db, struct in_addr ip);

/**
 * @brief Find a lease by MAC address.
 * @param db Pointer to the lease database structure.
 * @param mac MAC address to search for (6 bytes).
 * @return Pointer to the lease if found, NULL otherwise.
 *
 * Returns the first active lease for this MAC address.
 * A client may have multiple leases in different states.
 */
struct dhcp_lease_t *lease_db_find_by_mac(struct lease_database_t *db, const uint8_t mac[6]);

/**
 * @brief Release a lease (mark as FREE).
 * @param db Pointer to the lease database structure.
 * @param ip IP address of the lease to release.
 * @return 0 on success, -1 on failure.
 *
 * Called when a client sends DHCPRELEASE. Transitions the lease
 * to FREE state and updates timestamps.
 */
int lease_db_release_lease(struct lease_database_t *db, struct in_addr ip);

/**
 * @brief Renew an existing lease.
 * @param db Pointer to the lease database structure.
 * @param ip IP address of the lease to renew.
 * @param lease_time New lease duration in seconds.
 * @return 0 on success, -1 on failure.
 *
 * Extends the lease expiration time. Updates cltt (client last
 * transaction time) and recalculates end_time.
 */
int lease_db_renew_lease(struct lease_database_t *db, struct in_addr ip, uint32_t lease_time);

/**
 * @brief Mark expired leases as EXPIRED.
 * @param db Pointer to the lease database structure.
 * @return Number of leases expired.
 *
 * Scans all active leases and transitions expired ones to EXPIRED state.
 * Should be called periodically by the DHCP server main loop.
 */
int lease_db_expire_old_leases(struct lease_database_t *db);

/**
 * @brief Remove expired leases from the database.
 * @param db Pointer to the lease database structure.
 * @return Number of leases removed.
 *
 * Permanently removes leases in EXPIRED or FREE state to reclaim memory.
 * Use cautiously - this deletes lease history.
 */
int lease_db_cleanup_expired(struct lease_database_t *db);

/**
 * @brief Print lease database to stdout (for debugging).
 * @param db Pointer to the lease database structure.
 *
 * Displays all leases with their state, IP, MAC, and expiration times.
 * Useful for debugging and monitoring.
 */
void lease_db_print(const struct lease_database_t *db);

/**
 * @brief Convert lease state enum to string.
 * @param state Lease state enumeration value.
 * @return String representation (e.g., "active", "free", "expired").
 */
const char *lease_state_to_string(lease_state_t state);

/**
 * @brief Convert string to lease state enum.
 * @param str State string (e.g., "active", "free").
 * @return Lease state enumeration value.
 */
lease_state_t lease_state_from_string(const char *str);

/**
 * @brief Check if a lease has expired.
 * @param lease Pointer to the lease structure.
 * @return true if expired, false otherwise.
 *
 * Compares end_time with current time. Only checks ACTIVE leases.
 */
bool lease_is_expired(const struct dhcp_lease_t *lease);

/**
 * @brief Set client identifier (DHCP option 61) for a lease.
 * @param lease Pointer to the lease structure.
 * @param client_id Client identifier bytes.
 * @param len Length of client_id.
 * @return 0 on success, -1 on failure.
 *
 * Stores the client identifier sent in DHCP option 61.
 * Used for more reliable client identification than MAC address.
 */
int lease_set_client_id(struct dhcp_lease_t *lease, const uint8_t *client_id, uint32_t len);

/**
 * @brief Set vendor class identifier (DHCP option 60) for a lease.
 * @param lease Pointer to the lease structure.
 * @param vendor_class Vendor class string.
 * @return 0 on success, -1 on failure.
 *
 * Stores vendor identification (e.g., "MSFT 5.0", "Cisco Systems").
 * Useful for applying vendor-specific configurations.
 */
int lease_set_vendor_class(struct dhcp_lease_t *lease, const char *vendor_class);

/**
 * @brief Update lease timestamps to current time.
 * @param lease Pointer to the lease structure.
 * @param now Current Unix timestamp.
 *
 * Updates tstp (time state was put) and cltt (client last transaction time).
 */
void lease_update_timestamps(struct dhcp_lease_t *lease, time_t now);

/**
 * @brief Set state transition for a lease.
 * @param lease Pointer to the lease structure.
 * @param current Current binding state.
 * @param next Next binding state (after expiration).
 * @param rewind Rewind binding state (on failure/rollback).
 *
 * Configures the state machine for lease transitions.
 * Example: current=ACTIVE, next=FREE, rewind=FREE
 */
void lease_set_state_transition(struct dhcp_lease_t *lease, lease_state_t current, lease_state_t next, lease_state_t rewind);

/**
 * @brief Parse client ID from string format to binary.
 * @param str Client ID string (e.g., "\\001\\000\\021\\042").
 * @param client_id Output buffer for binary client ID.
 * @param len Output length of parsed client ID.
 * @return 0 on success, -1 on failure.
 *
 * Parses ISC DHCP lease file format for client identifiers.
 * Handles both hex escape sequences and quoted strings.
 */
int parse_client_id_from_string(const char *str, uint8_t *client_id, uint32_t *len);

/**
 * @brief Format client ID from binary to string representation.
 * @param client_id Binary client identifier.
 * @param len Length of client_id.
 * @param output Output buffer for formatted string.
 * @param output_len Size of output buffer.
 *
 * Formats client ID for writing to lease file in ISC DHCP format.
 */
void format_client_id_to_string(const uint8_t *client_id, uint32_t len, char *output, size_t output_len);

/**
 * @brief Parse timestamp from ISC DHCP lease file format.
 * @param time_str Time string (e.g., "4 2024/10/26 14:30:00").
 * @return Unix timestamp, or 0 on parse error.
 *
 * Parses ISC DHCP date format: <weekday> <YYYY/MM/DD> <HH:MM:SS>
 * Example: "4 2024/10/26 14:30:00" or "epoch 1698334200"
 */
time_t parse_lease_time(const char *time_str);

/**
 * @brief Format Unix timestamp to ISC DHCP lease file format.
 * @param timestamp Unix timestamp to format.
 * @param output Output buffer for formatted time string.
 * @param output_len Size of output buffer.
 *
 * Formats timestamp as: <weekday> <YYYY/MM/DD> <HH:MM:SS>
 * Example output: "4 2024/10/26 14:30:00"
 */
void format_lease_time(time_t timestamp, char *output, size_t output_len);

#endif // LEASE_V4_H