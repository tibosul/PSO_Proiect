#ifndef PD_POOL_H
#define PD_POOL_H

#include <stdint.h>
#include <time.h>
#include <netinet/in.h>
#include <stdbool.h>

#include "config_v6.h"
#include "leases6.h"
#include "ip6_pool.h"   

#define MAX_PD_POOL_SIZE 1024


/**
 * @brief Single Prefix Delegation (PD) pool entry.
 *
 * Represents one delegatable IPv6 prefix chunk (prefix/plen) and its state.
 * The entry stores the owning client DUID (hex string form, as used by your code),
 * allocation timestamp, and pool state (available/allocated/reserved/conflict).
 */
typedef struct pd_pool_entry_t {
    struct in6_addr prefix;    /**< Delegated prefix base address (binary). */
    uint8_t         plen;      /**< Delegated prefix length. */
    ip6_state_t     state;     /**< Pool state (available/allocated/reserved/conflict). */
    char            duid[DUID_MAX_LEN]; /**< Owner DUID in hex string form ("" if none). */
    time_t          last_allocated; /**< Last allocation time (0 if never allocated). */
} pd_pool_entry_t;


/**
 * @brief Prefix Delegation pool.
 *
 * The PD pool is derived from a subnet's PD range (pd_pool_start_bin .. pd_pool_end_bin).
 * It is split into fixed-size delegated prefixes (delegated_plen). Each entry corresponds
 * to one chunk that can be leased as IA_PD.
 */
typedef struct pd_pool_t {
    dhcpv6_subnet_t *subnet; /**< Subnet this pool belongs to. */

    struct in6_addr base_prefix;    /**< Base prefix for the pool (binary). */
    uint8_t         base_plen;      /**< Base prefix length. */
    uint8_t         delegated_plen; /**< Delegated prefix length. */

    pd_pool_entry_t entries[MAX_PD_POOL_SIZE];   /**< Pool entries (prefix chunks). */
    uint32_t        pool_size;      /**< Number of entries in the pool. */

    uint32_t        available_count;  /**< Number of available entries. */
    uint32_t        allocated_count;  /**< Number of allocated entries. */
    uint32_t        reserved_count;   /**< Number of reserved entries. */
} pd_pool_t;

/**
 * @brief Result of a PD allocation attempt.
 *
 * Contains the result of a PD allocation attempt, including success status,
 * allocated prefix, prefix length, and error message (if applicable).
 */
typedef struct pd_allocation_result_t {
    bool            success;    /**< true if allocation succeeded. */
    bool            is_new;
    struct in6_addr prefix;   /**< Allocated prefix (binary). */
    uint8_t         plen;     /**< Allocated prefix length. */
    char            error_message[256];  /**< Error message (if allocation failed). */
} pd_allocation_result_t;

/**
 * @brief Initialize a PD pool.
 *
 * Initializes a PD pool based on the subnet's PD range and delegated prefix length.
 * Populates the pool with available prefixes and syncs with the lease database.
 *
 * @param pool The PD pool to initialize.
 * @param subnet The subnet this pool belongs to.
 * @param db The lease database to sync with.
 * @param delegated_plen The delegated prefix length.
 * @return 0 on success, -1 on failure.
 */
int  pd_pool_init(pd_pool_t *pool, 
                  dhcpv6_subnet_t *subnet, 
                  lease_v6_db_t *db, 
                  uint8_t delegated_plen);

/**
 * @brief Free a PD pool.
 *
 * Frees a PD pool and sets all its fields to 0.
 *
 * @param pool The PD pool to free.
 */
void pd_pool_free(pd_pool_t *pool);

/**
 * @brief Find a PD pool entry.
 *
 * Finds a PD pool entry based on the prefix and prefix length.
 *
 * @param pool The PD pool to search in.
 * @param prefix The prefix to search for.
 * @param plen The prefix length to search for.
 * @return The PD pool entry if found, NULL otherwise.
 */
pd_pool_entry_t* pd_pool_find_entry(pd_pool_t *pool,
                                    const struct in6_addr *prefix,
                                    uint8_t plen);

/**
 * @brief Check if a PD pool entry is available.
 *
 * Checks if a PD pool entry is available for allocation.
 *
 * @param pool The PD pool to check.
 * @param prefix The prefix to check.
 * @param plen The prefix length to check.
 * @return true if the entry is available, false otherwise.
 */
bool pd_pool_is_available(pd_pool_t *pool, const struct in6_addr *prefix, uint8_t plen);

/**
 * @brief Allocate a PD pool entry.
 *
 * Allocates a PD pool entry for a client.
 *
 * @param pool The PD pool to allocate from.
 * @param duid_hex The client's DUID in hex string form.
 * @param duid_len The length of the DUID.
 * @param iaid The IAID for the allocation.
 * @param hostname_opt The hostname option for the allocation.
 * @param db The lease database to sync with.
 * @param lease_time The lease time for the allocation.
 * @return The result of the allocation attempt.
 */
pd_allocation_result_t pd_pool_allocate(pd_pool_t *pool,
                                        const char *duid_hex,
                                        uint16_t duid_len,
                                        uint32_t iaid,
                                        const char *hostname_opt,
                                        lease_v6_db_t *db,
                                        uint32_t lease_time);

/**
 * @brief Release a PD pool entry.
 *
 * Releases a PD pool entry for a client.
 *
 * @param pool The PD pool to release from.
 * @param prefix The prefix to release.
 * @param plen The prefix length to release.
 * @param db The lease database to sync with.
 * @return 0 on success, -1 on failure.
 */
int  pd_pool_release(pd_pool_t *pool,
                     const struct in6_addr *prefix,
                     uint8_t plen,
                     lease_v6_db_t *db);

/**
 * @brief Print statistics for a PD pool.
 *
 * Prints statistics for a PD pool, including the number of available,
 * allocated, reserved, and conflict entries.
 *
 * @param pool The PD pool to print statistics for.
 */
void pd_pool_print_stats(const pd_pool_t *pool);

/**
 * @brief Print detailed information for a PD pool.
 *
 * Prints detailed information for a PD pool, including the prefix,
 * prefix length, state, and owner DUID.
 *
 * @param pool The PD pool to print detailed information for.
 */
void pd_pool_print_detailed(const pd_pool_t *pool);

#endif //PD_POOL_H