#ifndef IP6_POOL_H
#define IP6_POOL_H

#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <netinet/in.h>

#include "config_v6.h"
#include "leases6.h"

#define MAX_POOL6_SIZE 4096


typedef enum
{
    IP6_STATE_AVAILABLE = 0,
    IP6_STATE_ALLOCATED,
    IP6_STATE_RESERVED,
    IP6_STATE_EXCLUDED, /* rezervat pentru viitor (IPv6 nu are broadcast/network ca în v4) */
    IP6_STATE_CONFLICT,
    IP6_STATE_UNKNOWN
} ip6_state_t;


struct ip6_pool_entry_t
{
    struct in6_addr ip_address;
    ip6_state_t state;
    char duid[DUID_MAX_LEN];
    time_t  last_allocated;
    uint64_t lease_id; 
};


struct ip6_pool_t
{
    dhcpv6_subnet_t* subnet;
    struct ip6_pool_entry_t entries[MAX_POOL6_SIZE];
    uint32_t pool_size;
    uint32_t available_count;
    uint32_t allocated_count;
    uint32_t reserved_count; 
};

struct ip6_allocation_result_t
{
    bool success;
    struct in6_addr ip_address;
    char error_message[256];

    bool err_is_conflict;
    struct in6_addr conflict_ip;
    const char* conflict_reason;
};

int  ip6_pool_init(struct ip6_pool_t* pool, dhcpv6_subnet_t* subnet, lease_v6_db_t* lease_db);
void ip6_pool_free(struct ip6_pool_t* pool);

int  ip6_pool_sync_with_leases(struct ip6_pool_t* pool, lease_v6_db_t* lease_db);
int  ip6_pool_update_from_lease(struct ip6_pool_t* pool, dhcpv6_lease_t* lease);

struct ip6_allocation_result_t ip6_pool_allocate(struct ip6_pool_t* pool, const char* duid, uint16_t duid_len, uint32_t iaid, const char*hostname_opt, struct in6_addr requested_ip, dhcpv6_config_t* config, lease_v6_db_t* lease_db, uint32_t lease_time);   

int  ip6_pool_reserve_ip(struct ip6_pool_t* pool, struct in6_addr ip, const char* duid);
int  ip6_pool_release_ip(struct ip6_pool_t* pool, struct in6_addr ip, lease_v6_db_t* db);
int  ip6_pool_mark_conflict(struct ip6_pool_t* pool, struct in6_addr ip, lease_v6_db_t* db, const char* reason);

bool ip6_pool_is_available(struct ip6_pool_t* pool, struct in6_addr ip);
bool ip6_pool_is_in_range(struct ip6_pool_t* pool, struct in6_addr ip);
struct ip6_pool_entry_t* ip6_pool_find_entry(struct ip6_pool_t* pool, struct in6_addr ip);

bool ip6_ping_check(struct in6_addr ip, uint32_t timeout_ms);

void ip6_pool_print_stats(const struct ip6_pool_t* pool);
void ip6_pool_print_detailed(const struct ip6_pool_t* pool);

ip6_state_t ip6_state_from_lease_state(lease_state_t lease_state);
const char* ip6_state_to_string(ip6_state_t state);

#endif /* IP6_POOL_H */