#ifndef IP6_POOL_H
#define IP6_POOL_H

#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <netinet/in.h>

#include "config_v6.h"
#include "leases6.h"

#define MAX_POOL6_SIZE 4096


/**
 * @brief IPv6 pool entry state.
 *
 * Describes how an IPv6 address is currently treated by the allocator.
 */
typedef enum
{
    IP6_STATE_AVAILABLE = 0,     /**< Free address, can be allocated. */
    IP6_STATE_ALLOCATED,         /**< Address is currently assigned to a client. */
    IP6_STATE_RESERVED,          /**< Address is reserved for future use. */
    IP6_STATE_EXCLUDED,          /**< Address is excluded from allocation. */
    IP6_STATE_CONFLICT,          /**< Address is in conflict with another lease. */
    IP6_STATE_UNKNOWN            /**< Address state is unknown. */
} ip6_state_t;


/**
 * @brief Represents a single IPv6 address entry in the pool.
 */
struct ip6_pool_entry_t
{
    struct in6_addr ip_address;  /**< IPv6 address (binary). */
    ip6_state_t state;           /**< Current state of the address. */
    char duid[DUID_MAX_LEN];      /**< DUID of the client (as parsed from config). */
    time_t  last_allocated;      /**< Timestamp of the last allocation. */
    uint64_t lease_id;           /**< Lease ID associated with the address. */
};

/**
 * @brief IPv6 pool container for a specific subnet.
 *
 * Holds all generated pool entries derived from subnet pool_start/pool_end and
 * keeps counters for fast statistics.
 */
struct ip6_pool_t
{
    dhcpv6_subnet_t* subnet;    /**< Owning subnet configuration. */
    struct ip6_pool_entry_t entries[MAX_POOL6_SIZE];   /**< Pool entries (capped). */
    uint32_t pool_size;         /**< Number of entries in the pool. */
    uint32_t available_count;   /**< Number of available addresses. */
    uint32_t allocated_count;   /**< Number of allocated addresses. */
    uint32_t reserved_count;    /**< Number of reserved addresses. */
};


/**
 * @brief Result returned by IPv6 allocation attempts.
 *
 * On success, @ref success is true and @ref ip_address contains the allocated address.
 * On failure, @ref error_message contains a human-readable error.
 * If a conflict was detected, @ref err_is_conflict is true and conflict fields are filled.
 */
struct ip6_allocation_result_t
{
    bool success;   /**< True if allocation succeeded. */
    bool is_new;
    struct in6_addr ip_address;   /**< Allocated IPv6 address (binary). */
    char error_message[256];       /**< Error message on failure. */

    bool err_is_conflict;          /**< True if error is a conflict. */
    struct in6_addr conflict_ip;   /**< Conflicting IP address. */
    const char* conflict_reason;   /**< Reason for conflict. */
};


/**
 * @brief Initialize an IPv6 pool for a subnet.
 *
 * Builds the pool entries from subnet->pool_start_bin to subnet->pool_end_bin (inclusive),
 * caps the pool at MAX_POOL6_SIZE, and optionally synchronizes states with an existing
 * lease database. Also reserves static host fixed addresses found in subnet->hosts[].
 *
 * @param pool     Pool object to initialize (output).
 * @param subnet   Subnet configuration providing pool range and static hosts.
 * @param lease_db Optional lease database used to sync existing allocations (may be NULL).
 * @return 0 on success, -1 on error (e.g., missing/invalid pool range).
 */
int  ip6_pool_init(struct ip6_pool_t* pool, dhcpv6_subnet_t* subnet, lease_v6_db_t* lease_db);

/**
 * @brief Reset/free an IPv6 pool structure.
 *
 * This implementation clears the pool structure in-place.
 *
 * @param pool Pool to reset.
 */
void ip6_pool_free(struct ip6_pool_t* pool);

/**
 * @brief Synchronize pool entry states from the lease database.
 *
 * Iterates through lease records and updates corresponding pool entries.
 *
 * @param pool     Pool to update.
 * @param lease_db Lease database containing leases to apply.
 * @return 0 on success, -1 on invalid parameters.
 */
int  ip6_pool_sync_with_leases(struct ip6_pool_t* pool, lease_v6_db_t* lease_db);

/**
 * @brief Update a single pool entry from a lease record.
 *
 * Maps the lease state to an ip6_state_t and updates counters accordingly.
 *
 * @param pool  Pool to update.
 * @param lease Lease record used to update a matching entry.
 * @return 0 on success (including "not found" entry), -1 on invalid parameters.
 */
int  ip6_pool_update_from_lease(struct ip6_pool_t* pool, dhcpv6_lease_t* lease);

/**
 * @brief Allocate an IPv6 address for a client (IA_NA).
 *
 * Allocation order (as implemented):
 * 1) If client matches a static host entry by DUID -> allocate its fixed address.
 * 2) If client already has an allocated address in this pool -> return it.
 * 3) If client requested a specific address and it is available -> allocate it.
 * 4) Otherwise allocate the first available address from the pool.
 *
 * When probing is enabled, an ICMPv6 echo check may be performed before allocation
 * and conflicts will be marked via @ref ip6_pool_mark_conflict.
 *
 * On successful allocation, a lease is persisted to the lease DB (IA_NA).
 *
 * @param pool         Pool from which to allocate.
 * @param duid         Client DUID (binary or string as used by your lease DB helper).
 * @param duid_len     Length of the DUID (used by lease DB).
 * @param iaid         IAID for IA_NA (used by lease DB).
 * @param hostname_opt Optional hostname from DHCPv6 option (may be NULL).
 * @param requested_ip Client requested IPv6 (use :: / unspecified to mean "no request").
 * @param config       Full DHCPv6 config (used for effective ICMPv6 probe settings).
 * @param lease_db     Lease database for persisting allocation.
 * @param lease_time   Lease lifetime (seconds).
 * @return Allocation result struct (success flag + address or error details).
 */
struct ip6_allocation_result_t ip6_pool_allocate(struct ip6_pool_t* pool, const char* duid, uint16_t duid_len, uint32_t iaid, const char*hostname_opt, struct in6_addr requested_ip, dhcpv6_config_t* config, lease_v6_db_t* lease_db, uint32_t lease_time);   

/**
 * @brief Reserve a specific IPv6 address in the pool (e.g., static host).
 *
 * Marks the entry as RESERVED and optionally associates it with a DUID string.
 *
 * @param pool Pool to modify.
 * @param ip   IPv6 address to reserve.
 * @param duid Optional DUID string to associate (may be NULL).
 * @return 0 on success, -1 if the entry was not found or invalid params.
 */
int  ip6_pool_reserve_ip(struct ip6_pool_t* pool, struct in6_addr ip, const char* duid);

/**
 * @brief Release an allocated IPv6 address back to AVAILABLE.
 *
 * Also updates the lease database accordingly (if provided).
 *
 * @param pool Pool to modify.
 * @param ip   IPv6 address to release.
 * @param db   Lease database (may be NULL).
 * @return 0 on success, -1 if the entry was not found or invalid params.
 */
int  ip6_pool_release_ip(struct ip6_pool_t* pool, struct in6_addr ip, lease_v6_db_t* db);

/**
 * @brief Mark an IPv6 address as CONFLICT.
 *
 * Updates internal counters and optionally marks the lease DB record as conflict.
 *
 * @param pool   Pool to modify.
 * @param ip     IPv6 address that is conflicting.
 * @param db     Lease database (may be NULL).
 * @param reason Human-readable reason (may be NULL).
 * @return 0 on success, -1 if entry not found or invalid params.
 */
int  ip6_pool_mark_conflict(struct ip6_pool_t* pool, struct in6_addr ip, lease_v6_db_t* db, const char* reason);

/**
 * @brief Check whether an IPv6 address is AVAILABLE in the pool.
 *
 * @param pool Pool to query.
 * @param ip   IPv6 address to test.
 * @return true if address exists in pool and state is AVAILABLE, otherwise false.
 */
bool ip6_pool_is_available(struct ip6_pool_t* pool, struct in6_addr ip);

/**
 * @brief Check whether an IPv6 address is within the subnet pool range.
 *
 * Range is defined by subnet->pool_start_bin and subnet->pool_end_bin.
 *
 * @param pool Pool to query.
 * @param ip   IPv6 address to test.
 * @return true if ip is in [pool_start, pool_end], otherwise false.
 */
bool ip6_pool_is_in_range(struct ip6_pool_t* pool, struct in6_addr ip);

/**
 * @brief Find the pool entry that matches a given IPv6 address.
 *
 * @param pool Pool to search.
 * @param ip   IPv6 address to find.
 * @return Pointer to the pool entry, or NULL if not found.
 */
struct ip6_pool_entry_t* ip6_pool_find_entry(struct ip6_pool_t* pool, struct in6_addr ip);

/**
 * @brief Perform an ICMPv6 echo probe to detect address conflicts.
 *
 * Sends an ICMPv6 Echo Request and waits for an Echo Reply within the timeout.
 * If a reply from the same IPv6 address is received, the address is considered
 * "in use" (conflict).
 *
 * @param ip         IPv6 address to probe.
 * @param timeout_ms Receive timeout in milliseconds.
 * @return true if a conflict is detected (echo reply received), false otherwise.
 */
bool ip6_ping_check(struct in6_addr ip, uint32_t timeout_ms);

/**
 * @brief Print statistics about the pool.
 *
 * @param pool Pool to print stats for.
 */
void ip6_pool_print_stats(const struct ip6_pool_t* pool);

/**
 * @brief Print detailed information about the pool.
 *
 * @param pool Pool to print detailed info for.
 */
void ip6_pool_print_detailed(const struct ip6_pool_t* pool);

/**
 * @brief Map a lease database state to an IPv6 pool state.
 *
 * @param lease_state Lease state from the lease subsystem.
 * @return Corresponding pool state.
 */
ip6_state_t ip6_state_from_lease_state(lease_state_t lease_state);

/**
 * @brief Convert an IPv6 pool state to a human-readable string.
 *
 * @param state Pool state to convert.
 * @return String representation of the state.
 */
const char* ip6_state_to_string(ip6_state_t state);

#endif /* IP6_POOL_H */