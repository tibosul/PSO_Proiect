#include "pd_pool.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "utilsv6.h"
#include "logger.h"

// Helper to increment IPv6 address at a specific bit position
// Example: incrementing 2001:db8:1:: at /48 will act on the bits from 0 to 47?
// Actually this function increments the value of the prefix treating it as an integer.
// But the `plen` argument specifies the boundary.
// If we have a /60 prefix, and we want to find the next /60, we increment at the 60th bit position.
//
// Logic:
// 1. Identify which byte contains the bit at `plen`.
// 2. Identify the bit offset within that byte.
// 3. Add 1 to that bit position, handling carry-over to previous bytes.
static void increment_prefix(struct in6_addr* ip, uint8_t plen) {
    if (plen == 0 || plen > 128) return;
    
    // We want to increment the prefix such that we get the next prefix of length `plen`.
    // Effectively, we are creating a sequence of prefixes: P, P + 2^(128-plen), ...
    // This is equivalent to adding 1 at the bit position `plen - 1`.
    
    int byte_idx = (plen - 1) / 8;
    int bit_in_byte = 7 - ((plen - 1) % 8);
    
    // We add 1 at the specific bit.
    // If bit_in_byte is 0 (LSB of byte), we add 1.
    // If bit_in_byte is 7 (MSB of byte), we add 128 (0x80).
    
    uint16_t add = 1 << bit_in_byte;
    
    // Working correctly with carry from the specified byte backwards
    for (int i = byte_idx; i >= 0; i--) {
        uint16_t val = (uint16_t)ip->s6_addr[i] + add;
        ip->s6_addr[i] = (uint8_t)(val & 0xFF);
        add = val >> 8; // Carry
        if (add == 0) break;
    }
}

int pd_pool_init(pd_pool_t *pool, dhcpv6_subnet_t *subnet, lease_v6_db_t *db, uint8_t delegated_plen) {
    if (!pool || !subnet) return -1;
    memset(pool, 0, sizeof(*pool));
    
    pool->subnet = subnet;
    pool->delegated_plen = delegated_plen;
    
    if (!subnet->pd_enabled || !subnet->has_pd_pool) {
        return 0;
    }
    
    struct in6_addr cur = subnet->pd_pool_start_bin;
    struct in6_addr end = subnet->pd_pool_end_bin;
    
    if (ipv6_compare(&cur, &end) > 0) {
        return -1;
    }

    pool->base_prefix = cur;
    pool->base_plen = delegated_plen;
    
    while (ipv6_compare(&cur, &end) <= 0) {
        if (pool->pool_size >= MAX_PD_POOL_SIZE) {
            fprintf(stderr, "PD Pool full, capping at %d\n", MAX_PD_POOL_SIZE);
            break;
        }
        
        pd_pool_entry_t* e = &pool->entries[pool->pool_size];
        e->prefix = cur;
        e->plen = delegated_plen;
        e->state = IP6_STATE_AVAILABLE;
        e->last_allocated = 0;
        e->duid[0] = '\0';
        
        pool->pool_size++;
        pool->available_count++;
        
        struct in6_addr next = cur;
        increment_prefix(&next, delegated_plen);
        
        if (ipv6_compare(&next, &cur) <= 0) break; // Overflow
        cur = next;
    }
    
    char pfx_start[INET6_ADDRSTRLEN], pfx_end[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &subnet->pd_pool_start_bin, pfx_start, sizeof(pfx_start));
    inet_ntop(AF_INET6, &subnet->pd_pool_end_bin, pfx_end, sizeof(pfx_end));
    log_info("PD Pool initialized: %s - %s / %d (Size: %u)", 
             pfx_start, pfx_end, delegated_plen, pool->pool_size);
    
    // Sync with DB
    if (db) {
        for (uint32_t i = 0; i < db->count; i++) {
            dhcpv6_lease_t* L = &db->leases[i];
            if (L->type != Lease6_IA_PD) continue;
            if (!L->in_use) continue;
            
            pd_pool_entry_t* e = pd_pool_find_entry(pool, &L->prefix_v6, L->plen);
            if (!e) continue;
            
            if (e->state == IP6_STATE_AVAILABLE && pool->available_count > 0) pool->available_count--;
            else if (e->state == IP6_STATE_ALLOCATED && pool->allocated_count > 0) pool->allocated_count--;
            
            e->state = ip6_state_from_lease_state(L->state);
            e->last_allocated = L->starts;
            
            if (L->duid_len > 0) {
                 duid_bin_to_hex(L->duid, L->duid_len, e->duid, sizeof(e->duid));
            }
            
            if (e->state == IP6_STATE_ALLOCATED) pool->allocated_count++;
            else if (e->state == IP6_STATE_AVAILABLE) pool->available_count++;
        }
    }
    
    return 0;
}

void pd_pool_free(pd_pool_t *pool) {
    if (pool) memset(pool, 0, sizeof(*pool));
}

pd_pool_entry_t* pd_pool_find_entry(pd_pool_t *pool, const struct in6_addr *prefix, uint8_t plen) {
    if (!pool || !prefix) return NULL;
    for (uint32_t i=0; i<pool->pool_size; i++) {
        pd_pool_entry_t* e = &pool->entries[i];
        if (e->plen == plen && ipv6_compare(&e->prefix, prefix) == 0) {
            return e;
        }
    }
    return NULL;
}

bool pd_pool_is_available(pd_pool_t *pool, const struct in6_addr *prefix, uint8_t plen) {
    pd_pool_entry_t* e = pd_pool_find_entry(pool, prefix, plen);
    return (e && e->state == IP6_STATE_AVAILABLE);
}

pd_allocation_result_t pd_pool_allocate(pd_pool_t *pool,
                                        const char *duid_hex,
                                        uint16_t duid_len,
                                        uint32_t iaid,
                                        const char *hostname_opt,
                                        lease_v6_db_t *db,
                                        uint32_t lease_time) {
    pd_allocation_result_t res = {0};
    if (!pool || !db) {
        snprintf(res.error_message, sizeof(res.error_message), "Invalid pool or db");
        return res;
    }
    
    // Check existing
    for (uint32_t i=0; i<pool->pool_size; i++) {
        pd_pool_entry_t* e = &pool->entries[i];
        if (e->state == IP6_STATE_ALLOCATED && strcmp(e->duid, duid_hex) == 0) {
             // Refresh lease
             if (!lease_v6_add_ia_pd(db, duid_hex, duid_len, iaid, &e->prefix, e->plen, lease_time, hostname_opt)) {
             }
             res.success = true;
             res.is_new = false;
             res.prefix = e->prefix;
             res.plen = e->plen;
             return res;
        }
    }
    
    // Find free
    pd_pool_entry_t* victim = NULL;
    for (uint32_t i=0; i<pool->pool_size; i++) {
        if (pool->entries[i].state == IP6_STATE_AVAILABLE) {
            victim = &pool->entries[i];
            break;
        }
    }
    
    if (!victim) {
        snprintf(res.error_message, sizeof(res.error_message), "No prefixes available");
        return res;
    }
    
    // Allocate
    victim->state = IP6_STATE_ALLOCATED;
    strncpy(victim->duid, duid_hex, sizeof(victim->duid)-1);
    victim->last_allocated = time(NULL);
    
    pool->available_count--;
    pool->allocated_count++;
    
    // Persist
    if (!lease_v6_add_ia_pd(db, duid_hex, duid_len, iaid, &victim->prefix, victim->plen, lease_time, hostname_opt)) {
        victim->state = IP6_STATE_AVAILABLE;
        victim->duid[0] = 0;
        pool->available_count++;
        pool->allocated_count--;
        snprintf(res.error_message, sizeof(res.error_message), "DB error");
        return res;
    }
    
    if (lease_v6_db_save(db) != 0) {
    }
    
    res.success = true;
    res.is_new = true;
    res.prefix = victim->prefix;
    res.plen = victim->plen;
    
    return res;
}

int pd_pool_release(pd_pool_t *pool, const struct in6_addr *prefix, uint8_t plen, lease_v6_db_t *db) {
     pd_pool_entry_t* e = pd_pool_find_entry(pool, prefix, plen);
     if (!e) return -1;
     
     if (e->state == IP6_STATE_ALLOCATED) {
         e->state = IP6_STATE_AVAILABLE;
         e->duid[0] = 0;
         if (pool->allocated_count > 0) pool->allocated_count--;
         pool->available_count++;
     }
     
     if (db) {
         lease_v6_release_prefix(db, prefix, plen);
         lease_v6_db_save(db);
     }
     return 0;
}

void pd_pool_print_stats(const pd_pool_t *pool) {
    if (!pool) return;
    printf("\n--- PD Pool Stats ---\n");
    printf("Total: %u, Avail: %u, Alloc: %u\n", 
           pool->pool_size, pool->available_count, pool->allocated_count);
}

void pd_pool_print_detailed(const pd_pool_t *pool) {
    if (!pool) return;
    printf("\n--- PD Pool Detailed ---\n");
    for (uint32_t i=0; i<pool->pool_size; i++) {
        const pd_pool_entry_t* e = &pool->entries[i];
        char pfx[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &e->prefix, pfx, sizeof(pfx));
        printf("%s/%d : %s [%s]\n", pfx, e->plen, ip6_state_to_string(e->state), e->duid);
    }
}
