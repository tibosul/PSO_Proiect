#ifndef IP_POOL_H
#define IP_POOL_H

#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <netinet/in.h>
#include "config_v4.h"
#include "lease_v4.h"

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

struct ip_pool_entry_t
{
    struct in_addr ip_address;
    ip_state_t state;
    uint8_t mac_address[6];
    time_t last_allocated;
    uint64_t lease_id;  // Lease ID reference (0 = no lease)
};

struct ip_pool_t
{
    struct dhcp_subnet_t* subnet;
    struct ip_pool_entry_t entries[MAX_POOL_SIZE];
    uint32_t pool_size;
    uint32_t available_count;
    uint32_t allocated_count;
};

struct ip_allocation_result_t
{
    bool success;
    struct in_addr ip_address;
    char error_message[256];
};

int ip_pool_init(struct ip_pool_t* pool, struct dhcp_subnet_t* subnet, struct lease_database_t* lease_db);
void ip_pool_free(struct ip_pool_t* pool);

struct ip_allocation_result_t ip_pool_allocate(struct ip_pool_t* pool, const uint8_t mac[6], struct in_addr requested_ip, struct dhcp_config_t* config);

int ip_pool_reserve_ip(struct ip_pool_t* pool, struct in_addr ip, const uint8_t mac[6]);
int ip_pool_release_ip(struct ip_pool_t* pool, struct in_addr ip);
int ip_pool_mark_conflict(struct ip_pool_t* pool, struct in_addr ip);

bool ip_pool_is_available(struct ip_pool_t* pool, struct in_addr ip);
bool ip_pool_is_in_range(struct ip_pool_t* pool, struct in_addr ip);
struct ip_pool_entry_t* ip_pool_find_entry(struct ip_pool_t* pool, struct in_addr ip);

bool ip_is_network_address(struct in_addr ip, struct in_addr network, struct in_addr netmask);
bool ip_is_broadcast_address(struct in_addr ip, struct in_addr network, struct in_addr netmask);
bool ip_is_gateway(struct in_addr ip, struct in_addr gateway);

bool ip_ping_check(struct in_addr ip, uint32_t timeout_ms);

void ip_pool_print_stats(const struct ip_pool_t* pool);
void ip_pool_print_detailed(const struct ip_pool_t* pool);

ip_state_t ip_state_from_string(const char* str);
const char* ip_state_to_string(ip_state_t state);

// State mapping and synchronization
ip_state_t ip_state_from_lease_state(lease_state_t lease_state);
int ip_pool_sync_with_leases(struct ip_pool_t* pool, struct lease_database_t* lease_db);
int ip_pool_update_from_lease(struct ip_pool_t* pool, struct dhcp_lease_t* lease);
struct dhcp_lease_t* ip_pool_allocate_and_create_lease(struct ip_pool_t* pool,
                                                        struct lease_database_t* lease_db,
                                                        const uint8_t mac[6],
                                                        struct in_addr requested_ip,
                                                        struct dhcp_config_t* config,
                                                        uint32_t lease_time);

#endif // IP_POOL_H