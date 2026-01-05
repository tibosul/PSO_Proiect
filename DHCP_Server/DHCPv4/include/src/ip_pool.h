#ifndef IP_POOL_H
#define IP_POOL_H

#include "config_v4.h"
#include "lease_v4.h"
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define MAX_POOL_SIZE 1024

typedef enum
{
  IP_STATE_AVAILABLE = 0,
  IP_STATE_ALLOCATED,
  IP_STATE_RESERVED,
  IP_STATE_EXCLUDED,
  IP_STATE_CONFLICT,
  IP_STATE_UNKNOWN
} ip_state_t;

/**
 * @brief Convert IP state enum to string.
 * @param state IP state enum value.
 * @return String representation of the IP state.
 */
ip_state_t ip_state_from_string(const char *str);

/**
 * @brief Convert string to IP state enum.
 * @param state IP state enum value.
 * @return String representation of the IP state.
 */
const char *ip_state_to_string(ip_state_t state);

struct ip_pool_entry_t
{
  struct in_addr ip_address;
  ip_state_t state;
  uint8_t mac_address[6];
  time_t last_allocated;
  uint64_t lease_id; // Lease ID reference (0 = no lease)
};

struct ip_pool_t
{
  struct dhcp_subnet_t *subnet;
  struct ip_pool_entry_t entries[MAX_POOL_SIZE];
  uint32_t pool_size;
  uint32_t allocated_count;
  uint32_t available_count;

  pthread_mutex_t mutex;
};

struct ip_allocation_result_t
{
  bool success;
  struct in_addr ip_address;
  char error_message[256];
};

/**
 * @brief Initialize an IP pool for the given subnet.
 * @param pool Pointer to ip_pool_t structure to initialize.
 * @param subnet Pointer to dhcp_subnet_t defining the subnet range.
 * @param lease_db Pointer to lease_database_t for syncing existing leases.
 * @return 0 on success, -1 on failure (e.g., invalid parameters).
 */
int ip_pool_init(struct ip_pool_t *pool, struct dhcp_subnet_t *subnet, struct lease_database_t *lease_db);

/**
 * @brief Free resources associated with the IP pool.
 * @param pool Pointer to ip_pool_t structure to free.
 */
void ip_pool_free(struct ip_pool_t *pool);

/**
 * @brief Allocate an IP address from the pool for the given MAC address.
 * @param pool Pointer to ip_pool_t structure.
 * @param mac Pointer to 6-byte MAC address of the client.
 * @param requested_ip Requested IP address (if available).
 * @param config Pointer to dhcp_config_t for configuration options.
 * @return ip_allocation_result_t structure with allocation result.
 */
struct ip_allocation_result_t ip_pool_allocate(struct ip_pool_t *pool, const uint8_t mac[6],
                                               struct in_addr requested_ip, struct dhcp_config_t *config);

/**
 * @brief Reserve a specific IP address for a client MAC address.
 * @param pool Pointer to ip_pool_t structure.
 * @param ip IP address to reserve.
 * @param mac Pointer to 6-byte MAC address of the client.
 * @return 0 on success, -1 on failure (e.g., IP not in pool or already reserved).
 */
int ip_pool_reserve_ip(struct ip_pool_t *pool, struct in_addr ip, const uint8_t mac[6]);

/**
 * @brief Release an allocated IP address back to the pool.
 * @param pool Pointer to ip_pool_t structure.
 * @param ip IP address to release.
 * @return 0 on success, -1 on failure (e.g., IP not found or not allocated).
 */
int ip_pool_release_ip(struct ip_pool_t *pool, struct in_addr ip);

/**
 * @brief Mark an IP address as in conflict (e.g., ping check failed).
 * @param pool Pointer to ip_pool_t structure.
 * @param ip IP address to mark as conflict.
 * @return 0 on success, -1 on failure (e.g., IP not found).
 */
int ip_pool_mark_conflict(struct ip_pool_t *pool, struct in_addr ip);

/**
 * @brief Check if an IP address is available for allocation.
 * @param pool Pointer to ip_pool_t structure.
 * @param ip IP address to check.
 * @return true if available, false otherwise.
 */
bool ip_pool_is_available(struct ip_pool_t *pool, struct in_addr ip);

/**
 * @brief Check if an IP address is within the pool's subnet range.
 * @param pool Pointer to ip_pool_t structure.
 * @param ip IP address to check.
 * @return true if in range, false otherwise.
 */
bool ip_pool_is_in_range(struct ip_pool_t *pool, struct in_addr ip);

/**
 * @brief Find the pool entry for a given IP address.
 * @param pool Pointer to ip_pool_t structure.
 * @param ip IP address to find.
 * @return Pointer to ip_pool_entry_t if found, NULL otherwise.
 */
struct ip_pool_entry_t *ip_pool_find_entry(struct ip_pool_t *pool, struct in_addr ip);

/**
 * @brief Check if an IP address is the network address of the subnet.
 * @param ip IP address to check.
 * @param network Network address of the subnet.
 * @param netmask Netmask of the subnet.
 * @return true if it is the network address, false otherwise.
 */

 /**
  * @brief Check if an IP address is the broadcast address of the subnet.
  * @param ip IP address to check.
  * @param network Network address of the subnet.
  * @param netmask Netmask of the subnet.
  * @return true if it is the broadcast address, false otherwise.
  */
bool ip_is_network_address(struct in_addr ip, struct in_addr network, struct in_addr netmask);

/**
 * @brief Check in an IP address is the broadcast address of the subnet.
 * @param ip IP address to check.
 * @param network Network address of the subnet.
 * @param netmask Netmask of the subnet.
 * @return true if it is the broadcast address, false otherwise.
 */
bool ip_is_broadcast_address(struct in_addr ip, struct in_addr network, struct in_addr netmask);

/** @brief Check if an IP address matches the gateway address.
 *  @param ip IP address to check.
 *  @param gateway Gateway address of the subnet.
 *  @return true if it matches the gateway, false otherwise.
 */
bool ip_is_gateway(struct in_addr ip, struct in_addr gateway);

/**
 * @brief Perform a ping check on the given IP address.
 * @param ip IP address to ping.
 * @param timeout_ms Timeout in milliseconds.
 * @return true if the IP responds to ping, false otherwise.
 */
bool ip_ping_check(struct in_addr ip, uint32_t timeout_ms);

/**
 * @brief Print summary statistics of the IP pool.
 * @param pool Pointer to ip_pool_t structure.
 */
void ip_pool_print_stats(const struct ip_pool_t *pool);

/**
 * @brief Print detailed information of all IP pool entries.
 * @param pool Pointer to ip_pool_t structure.
 */
void ip_pool_print_detailed(const struct ip_pool_t *pool);

/**
 * @brief Map lease state to IP state.
 * @param lease_state Lease state enum value.
 * @return Corresponding IP state enum value.
 */
ip_state_t ip_state_from_lease_state(lease_state_t lease_state);

/**
 * @brief Synchronize the IP pool with the lease database.
 * @param pool Pointer to ip_pool_t structure.
 * @param lease_db Pointer to lease_database_t structure.
 * @return 0 on success, -1 on failure.
 */
int ip_pool_sync_with_leases(struct ip_pool_t *pool, struct lease_database_t *lease_db);

/**
 * @brief Update the IP pool entry based on a lease record.
 * @param pool Pointer to ip_pool_t structure.
 * @param lease Pointer to dhcp_lease_t structure.
 * @return 0 on success, -1 on failure.
 */
int ip_pool_update_from_lease(struct ip_pool_t *pool, struct dhcp_lease_t *lease);

/**
 * @brief Allocate an IP address and create or renew a lease in the lease database.
 * @param pool Pointer to ip_pool_t structure.
 * @param lease_db Pointer to lease_database_t structure.
 * @param mac Pointer to 6-byte MAC address of the client.
 * @param requested_ip Requested IP address (if available).
 * @param config Pointer to dhcp_config_t for configuration options.
 * @param lease_time Lease time in seconds.
 * @return Pointer to dhcp_lease_t structure on success, NULL on failure.
 */
struct dhcp_lease_t *ip_pool_allocate_and_create_lease(struct ip_pool_t *pool, struct lease_database_t *lease_db,
                                                       const uint8_t mac[6], struct in_addr requested_ip,
                                                       struct dhcp_config_t *config, uint32_t lease_time);

#endif // IP_POOL_H