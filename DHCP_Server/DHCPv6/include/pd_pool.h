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

typedef struct pd_pool_entry_t {
    struct in6_addr prefix;   
    uint8_t         plen;     
    ip6_state_t     state;   
    char            duid[DUID_MAX_LEN]; 
    time_t          last_allocated;
} pd_pool_entry_t;

typedef struct pd_pool_t {
    dhcpv6_subnet_t *subnet;

    struct in6_addr base_prefix;    
    uint8_t         base_plen;      
    uint8_t         delegated_plen; 

    pd_pool_entry_t entries[MAX_PD_POOL_SIZE];
    uint32_t        pool_size;

    uint32_t        available_count;
    uint32_t        allocated_count;
    uint32_t        reserved_count;
} pd_pool_t;

typedef struct pd_allocation_result_t {
    bool            success;
    struct in6_addr prefix;
    uint8_t         plen;
    char            error_message[256];
} pd_allocation_result_t;

int  pd_pool_init(pd_pool_t *pool,
                  dhcpv6_subnet_t *subnet,
                  lease_v6_db_t *db,
                  uint8_t delegated_plen);

void pd_pool_free(pd_pool_t *pool);

pd_pool_entry_t* pd_pool_find_entry(pd_pool_t *pool,
                                    const struct in6_addr *prefix,
                                    uint8_t plen);

bool pd_pool_is_available(pd_pool_t *pool,
                          const struct in6_addr *prefix,
                          uint8_t plen);

pd_allocation_result_t pd_pool_allocate(pd_pool_t *pool,
                                        const char *duid_hex,
                                        uint16_t duid_len,
                                        uint32_t iaid,
                                        const char *hostname_opt,
                                        lease_v6_db_t *db,
                                        uint32_t lease_time);

int  pd_pool_release(pd_pool_t *pool,
                     const struct in6_addr *prefix,
                     uint8_t plen,
                     lease_v6_db_t *db);

void pd_pool_print_stats(const pd_pool_t *pool);
void pd_pool_print_detailed(const pd_pool_t *pool);

#endif //PD_POOL_H