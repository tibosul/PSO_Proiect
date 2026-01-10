#ifndef LEASES6_H
#define LEASES6_H

#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <netinet/in.h>
#include "utilsv6.h"
#include "../../DHCPv4/include/utils/time_utils.h"

#define LEASES6_MAX 4096
#define DUID_MAX_LEN 128
#define IP6_STR_MAX 80
#define HOSTNAME6_MAX 128
#define LEASE6_PATH_MAX 512
#define LEASE_V6_STR_MAX       96
#define MAX_V6_VENDOR_CLASS_LEN 128
#define MAX_V6_FQDN_LEN 256

/**
 * @brief DHCPv6 lease type.
 *
 * IA_NA is a non-temporary address lease.
 * IA_PD is a prefix delegation lease.
 */
typedef enum {
    Lease6_IA_NA = 0,
    Lease6_IA_PD = 1
}lease_v6_type_t;

/**
 * @brief Lease binding state stored in the lease DB.
 */
typedef enum{
    LEASE_STATE_FREE = 0,
    LEASE_STATE_ACTIVE,
    LEASE_STATE_EXPIRED,
    LEASE_STATE_RELEASED,
    LEASE_STATE_ABANDONED,
    LEASE_STATE_RESERVED,
    LEASE_STATE_BACKUP
}lease_state_t;

/**
 * @brief Represents a DHCPv6 lease record (IA_NA or IA_PD).
 *
 * The struct stores client identification (DUID+IAID), address/prefix data,
 * timestamps, and binding states similarly to dhcpd.leases-style formats.
 */
typedef struct dhcpv6_lease_t{
    uint8_t in_use;  /**< 1 if entry is used/valid, 0 otherwise. */
    lease_v6_type_t type;   /**< IA_NA or IA_PD. */

    /** Client identification */
    uint8_t duid[DUID_MAX_LEN]  /**< Client DUID (binary). */;
    uint16_t duid_len;           /**< Length of DUID in bytes. */
    uint32_t iaid;               /**< IAID for IA_NA (used by lease DB). */

    /** IA NA */
    struct in6_addr ip6_addr;    /**< IPv6 address (binary). */
    char ip6_addr_str[IP6_STR_MAX];     /**< Text form of leased IPv6 address. */

    /** IA PD */
    struct in6_addr prefix_v6;   /**< IPv6 prefix (binary). */
    uint8_t plen;                /**< Prefix length. */
    char prefix_str[IP6_STR_MAX];

    /** Times */
    time_t starts;               /**< Start time of the lease. */
    time_t ends;                 /**< End time of the lease. */
    time_t tstp;                 /**< Time state was put. */
    time_t cltt;                 /**< Client last transaction time. */

    char client_hostname[HOSTNAME6_MAX];    /**< Optional client hostname. */
    lease_state_t state;         /**< Current lease state. */
    lease_state_t next_state;    /**< Next binding state. */
    lease_state_t rewind_state;  /**< Rewind binding state. */
    char binding_state[16];      /**< String form of binding state (as stored). */

    char vendor_class[MAX_V6_VENDOR_CLASS_LEN];  /**< Optional vendor class string. */
    char fqdn[MAX_V6_FQDN_LEN];                  /**< Optional FQDN string. */
}dhcpv6_lease_t;

/**
 * @brief Lease database container.
 *
 * Holds a fixed-size array of lease entries and the path of the backing file.
 */
typedef struct lease_v6_db_t{
    char filename[LEASE6_PATH_MAX];    /**< Path to the lease database file. */
    uint32_t count;                    /**< Number of leases in the database. */
    uint32_t capacity;                 /**< Maximum number of leases supported. */
    dhcpv6_lease_t leases[LEASES6_MAX]; /**< Array of lease records. */
}lease_v6_db_t;

/**
 * @brief Convert lease state enum to string.
 *
 * @param s Lease state enum value.
 * @return String representation of the lease state.
 */
const char* lease_v6_state_to_string(lease_state_t s);

/**
 * @brief Convert string to lease state enum.
 *
 * @param str String representation of the lease state.
 * @return Lease state enum value.
 */
lease_state_t lease_v6_state_from_string(const char* str);

/**
 * @brief Check if a lease is expired.
 *
 * @param lease Lease record to check.
 * @return True if the lease is expired, false otherwise.
 */
bool lease_v6_is_expired(const dhcpv6_lease_t* lease);

/**
 * @brief Initialize a lease database structure.
 *
 * Sets filename and resets counters. Does not load from disk automatically.
 *
 * @param db       Lease DB object to initialize.
 * @param filename Path to the lease DB file.
 * @return 0 on success, -1 on invalid parameters.
 */
int lease_v6_db_init(lease_v6_db_t *db, const char* filename);

/**
 * @brief Free resources associated with a lease database.
 *
 * @param db Lease DB object to free.
 */
void lease_v6_db_free(lease_v6_db_t *db);

/**
 * @brief Load leases from the database file into memory.
 *
 * @param db Lease DB object to load into.
 * @return 0 on success, -1 on failure.
 */
int lease_v6_db_load(lease_v6_db_t *db);

/**
 * @brief Save leases from memory to the database file.
 *
 * @param db Lease DB object to save from.
 * @return 0 on success, -1 on failure.
 */
int lease_v6_db_save(lease_v6_db_t *db);

/**
 * @brief Append a lease to the database.
 *
 * @param db     Lease DB object to append to.
 * @param lease  Lease record to append.
 * @return 0 on success, -1 on failure.
 */
int lease_v6_db_append(lease_v6_db_t *db, const dhcpv6_lease_t *lease);

/**
 * @brief Add a new IA_NA lease to the database.
 *
 * @param db           Lease DB object to add to.
 * @param duid         Client DUID (binary).
 * @param duid_len     Length of DUID in bytes.
 * @param iaid         IAID for IA_NA (used by lease DB).
 * @param ip6_addr     IPv6 address (binary).
 * @param lease_sec    Lease duration in seconds.
 * @param hostname     Optional client hostname.
 * @return Pointer to the added lease, or NULL on failure.
 */
dhcpv6_lease_t* lease_v6_add_ia_na(lease_v6_db_t *db, const char* duid, uint16_t duid_len, uint32_t iaid, const struct in6_addr* ip6_addr, uint32_t lease_sec, const char* hostname);

/**
 * @brief Add an IA_PD lease (delegated prefix) to the database.
 *
 * Creates a new ACTIVE lease entry for a delegated prefix and persists it (append).
 *
 * @param db        Lease DB.
 * @param duid      Client DUID (hex string format expected by implementation).
 * @param duid_len  DUID length indicator (used by some callers; implementation may re-parse duid).
 * @param iaid      Client IAID.
 * @param prefix_v6 Delegated prefix base address.
 * @param plen      Prefix length.
 * @param lease_sec Lease lifetime in seconds.
 * @param hostname  Optional hostname (may be NULL).
 * @return Pointer to the created lease on success, NULL on failure.
 */
dhcpv6_lease_t* lease_v6_add_ia_pd(lease_v6_db_t* db, const char* duid, uint16_t duid_len, uint32_t iaid, const struct in6_addr* prefix_v6, uint8_t plen, uint32_t lease_sec, const char* hostname);


/**
 * @brief Find an IA_NA lease by IPv6 address.
 *
 * @param db       Lease DB.
 * @param ip6_addr IPv6 address to search for.
 * @return Pointer to lease if found, otherwise NULL.
 */
dhcpv6_lease_t* lease_v6_find_by_ip(lease_v6_db_t *db, const struct in6_addr* ip6_addr);

/**
 * @brief Find an IA_PD lease by prefix.
 *
 * @param db       Lease DB.
 * @param prefix_v6 Prefix to search for.
 * @param plen     Prefix length.
 * @return Pointer to lease if found, otherwise NULL.
 */
dhcpv6_lease_t* lease_v6_find_by_prefix(lease_v6_db_t* db, const struct in6_addr* prefix_v6, uint8_t plen);

/**
 * @brief Find a lease by DUID and IAID.
 *
 * @param db       Lease DB.
 * @param duid     Client DUID (binary).
 * @param duid_len DUID length.
 * @param iaid     IAID.
 * @param type     Lease type (IA_NA or IA_PD).
 * @return Pointer to lease if found, otherwise NULL.
 */
dhcpv6_lease_t* lease_v6_find_by_duid_iaid(lease_v6_db_t *db, const uint8_t* duid, uint16_t duid_len, uint32_t iaid, lease_v6_type_t type);

/**
 * @brief Release an IA_NA lease by IPv6 address.
 *
 * @param db       Lease DB.
 * @param ip6_addr IPv6 address to release.
 * @return 0 on success, -1 on failure.
 */
int lease_v6_release_ip(lease_v6_db_t* db, const struct in6_addr* ip6_addr);

/**
 * @brief Release an IA_PD lease (mark as RELEASED and persist).
 *
 * @param db        Lease DB.
 * @param prefix_v6 Prefix to release.
 * @param plen      Prefix length.
 * @return 0 on success, -1 if not found or invalid params.
 */
int lease_v6_release_prefix(lease_v6_db_t* db, const struct in6_addr* prefix_v6, uint8_t plen);

/**
 * @brief Renew an IA_NA lease (mark as RENEWED and persist).
 *
 * @param db        Lease DB.
 * @param ip6_addr  IPv6 address to renew.
 * @param lease_sec New lease lifetime in seconds.
 * @return 0 on success, -1 if not found or invalid params.
 */
int lease_v6_renew_ip(lease_v6_db_t* db, const struct in6_addr* ip6_addr, uint32_t lease_sec);

/**
 * @brief Renew an IA_PD lease (mark as RENEWED and persist).
 *
 * @param db        Lease DB.
 * @param prefix_v6 Prefix to renew.
 * @param plen      Prefix length.
 * @param lease_sec New lease lifetime in seconds.
 * @return 0 on success, -1 if not found or invalid params.
 */
int lease_v6_renew_prefix(lease_v6_db_t* db, const struct in6_addr* prefix_v6, uint8_t plen, uint32_t lease_sec);

/**
 * @brief Mark expired leases older than a given time (mark as EXPIRED and persist).
 *
 * @param db Lease DB.
 * @return 0 on success, -1 on failure.
 */
int lease_v6_mark_expired_older(lease_v6_db_t* db);

/**
 * @brief Clean up expired leases (mark as EXPIRED and persist).
 *
 * @param db Lease DB.
 * @return 0 on success, -1 on failure.
 */
int lease_v6_cleanup(lease_v6_db_t* db);

/**
 * @brief Mark a lease as reserved (mark as RESERVED and persist).
 *
 * @param db        Lease DB.
 * @param ip6       IPv6 address to mark.
 * @param duid_hex  Client DUID (hex string).
 * @param iaid      IAID.
 * @param hostname  Optional hostname (may be NULL).
 * @return 0 on success, -1 on failure.
 */
int lease_v6_mark_reserved(lease_v6_db_t* db, const struct in6_addr* ip6, const char* duid_hex, uint32_t iaid, const char* hostname);

/**
 * @brief Print the contents of a lease database.
 *
 * @param lease Lease DB to print.
 */
void lease_v6_db_print(const lease_v6_db_t* lease);

/**
 * @brief Set the state of a lease (mark as state and persist).
 *
 * @param db        Lease DB.
 * @param ip6_addr  IPv6 address to set state for.
 * @param new_state New lease state.
 * @return 0 on success, -1 if not found or invalid params.
 */
int lease_v6_set_state(lease_v6_db_t* db, const struct in6_addr* ip6_addr, lease_state_t new_state);

/**
 * @brief Mark a lease as in conflict (mark as CONFLICT and persist).
 *
 * @param db        Lease DB.
 * @param ip6_addr  IPv6 address to mark.
 * @param reason    Reason for conflict (e.g., "DUPLICATE").
 * @return 0 on success, -1 on failure.
 */
int lease_v6_mark_conflict(lease_v6_db_t* db, const struct in6_addr* ip6_addr, const char* reason); 
#endif // LEASES6_H
