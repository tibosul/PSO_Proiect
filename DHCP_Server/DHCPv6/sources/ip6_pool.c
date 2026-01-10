#include "ip6_pool.h"
#include "utilsv6.h"
#include "logger.h"

#include <arpa/inet.h>
#include <netinet/icmp6.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <netinet/in.h>

static const char* S_UNKNOWN = "unknown";

const char* ip6_state_to_string(ip6_state_t s)
{
    switch (s) {
        case IP6_STATE_AVAILABLE: return "available";
        case IP6_STATE_ALLOCATED: return "allocated";
        case IP6_STATE_RESERVED:  return "reserved";
        case IP6_STATE_EXCLUDED:  return "excluded";
        case IP6_STATE_CONFLICT:  return "conflict";
        default: return S_UNKNOWN;
    }
}

ip6_state_t ip6_state_from_lease_state(lease_state_t s)
{
    switch (s) {
        case LEASE_STATE_ACTIVE:   return IP6_STATE_ALLOCATED;
        case LEASE_STATE_RESERVED: return IP6_STATE_RESERVED;
        case LEASE_STATE_ABANDONED:return IP6_STATE_CONFLICT;
        case LEASE_STATE_FREE:
        case LEASE_STATE_EXPIRED:
        case LEASE_STATE_RELEASED: return IP6_STATE_AVAILABLE;
        case LEASE_STATE_BACKUP:   return IP6_STATE_ALLOCATED; 
        default:                   return IP6_STATE_UNKNOWN;
    }
}

static void get_effective_probe(const dhcpv6_config_t* cfg,
                                const dhcpv6_subnet_t* sn,
                                bool* do_probe,
                                uint32_t* timeout_ms)
{
    bool enabled = cfg->global.icmp6_probe;
    uint32_t tmo = cfg->global.icmp6_timeout_ms ? cfg->global.icmp6_timeout_ms : 500;

    if (sn && sn->icmp6_probe_override) {
        enabled = sn->icmp6_probe;
        if (sn->has_icmp6_timeout) tmo = sn->icmp6_timeout_ms;
    }
    if (do_probe)   *do_probe   = enabled;
    if (timeout_ms) *timeout_ms = tmo;
}




bool ip6_ping_check(struct in6_addr ip, uint32_t timeout_ms)
{

    int s = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
    if (s < 0) {
        log_debug("icmp6 probe: cannot open raw socket (%s) -> assume free", strerror(errno));
        return false; 
    }

    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
   
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
         log_debug("icmp6 probe: setsockopt SO_RCVTIMEO failed (%s)", strerror(errno));
    }

    struct sockaddr_in6 dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin6_family = AF_INET6;
    dst.sin6_addr   = ip;
    const uint16_t ping_id  = (uint16_t)getpid();
    const uint16_t ping_seq = 1; 

    struct icmp6_hdr req;
    memset(&req, 0, sizeof(req));
    req.icmp6_type = ICMP6_ECHO_REQUEST;
    req.icmp6_code = 0;
    req.icmp6_id   = ping_id;
    req.icmp6_seq  = ping_seq;
   

    ssize_t sent = sendto(s, &req, sizeof(req), 0,
                          (struct sockaddr*)&dst, sizeof(dst));
                          
    if (sent < 0) {
        log_debug("icmp6 probe: sendto failed (%s) -> assume free", strerror(errno));
        close(s);
        return false; 
    }

    char recv_buf[512];
    bool conflict_detected = false;
   
    char ip_str_buf[INET6_ADDRSTRLEN];
    ipv6_bin_to_str(&ip, ip_str_buf, sizeof(ip_str_buf));

  
    for (int i = 0; i < 3; i++) 
    {
        struct sockaddr_in6 src_addr;
        socklen_t src_len = sizeof(src_addr);
        
        ssize_t n = recvfrom(s, recv_buf, sizeof(recv_buf), 0,
                             (struct sockaddr*)&src_addr, &src_len);
    
       

        if (n < 0) {
          
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                
                log_debug("icmp6 probe: recvfrom timeout for %s -> IP is free", ip_str_buf);
            } else {
                
                log_debug("icmp6 probe: recvfrom error (%s) -> assume free", strerror(errno));
            }
            break; 
        }

        if (memcmp(&src_addr.sin6_addr, &ip, sizeof(struct in6_addr)) != 0) {
            continue; 
        }

        if (n < (ssize_t)sizeof(struct icmp6_hdr)) {
            
            continue;
        }

        struct icmp6_hdr* reply = (struct icmp6_hdr*)recv_buf;

        
        if (reply->icmp6_type == ICMP6_ECHO_REPLY &&
            reply->icmp6_id   == ping_id &&
            reply->icmp6_seq  == ping_seq) 
        {
            
            conflict_detected = true;
            break; 
        }
        
        
    }

    close(s);
    
   
    if (conflict_detected) {
        log_debug("icmp6 probe: CONFLICT detected for %s", ip_str_buf);
    }

    return conflict_detected;
}


struct ip6_pool_entry_t* ip6_pool_find_entry(struct ip6_pool_t* pool, struct in6_addr ip)
{
    if (!pool) return NULL;
    for (uint32_t i = 0; i < pool->pool_size; ++i) {
        if (memcmp(&pool->entries[i].ip_address, &ip, sizeof(struct in6_addr)) == 0)
            return &pool->entries[i];
    }
    return NULL;
}

bool ip6_pool_is_in_range(struct ip6_pool_t* pool, struct in6_addr ip)
{
    if (!pool || !pool->subnet || !pool->subnet->has_pool_range) return false;
    const struct in6_addr* A = &pool->subnet->pool_start_bin;
    const struct in6_addr* B = &pool->subnet->pool_end_bin;

    return (ipv6_compare(&ip, A) >= 0 && ipv6_compare(&ip, B) <= 0);
}

bool ip6_pool_is_available(struct ip6_pool_t* pool, struct in6_addr ip)
{
    struct ip6_pool_entry_t* e = ip6_pool_find_entry(pool, ip);
    return (e && e->state == IP6_STATE_AVAILABLE);
}


int ip6_pool_init(struct ip6_pool_t* pool, dhcpv6_subnet_t* subnet, lease_v6_db_t* db)
{
    if (!pool || !subnet) return -1;
    memset(pool, 0, sizeof(*pool));
    pool->subnet = subnet;

    if (!subnet->has_pool_range) {
        log_error("ip6_pool_init: subnet has no pool range");
        return -1;
    }

    struct in6_addr cur = subnet->pool_start_bin;
    struct in6_addr end = subnet->pool_end_bin;

    char s_start[INET6_ADDRSTRLEN], s_end[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &cur, s_start, sizeof(s_start));
    inet_ntop(AF_INET6, &end, s_end, sizeof(s_end));
    int iterations = 0;

   while (ipv6_compare(&cur, &end) <= 0) {
        

        
        // Limit processing to prevent infinite loops on large ranges
        if (iterations++ > 50000) {
            log_warn("Pool init limit reached (50k IPs), stopping.");
            break;
        }

        if (pool->pool_size >= MAX_POOL6_SIZE) {
            log_info("Pool full (capped at %d)", MAX_POOL6_SIZE);
            break;
        }

        struct ip6_pool_entry_t* e = &pool->entries[pool->pool_size++];
        memset(e, 0, sizeof(*e));
        e->ip_address = cur;
        e->state = IP6_STATE_AVAILABLE;
        pool->available_count++;

        // Increment
        if (!ipv6_increment(&cur)) {
            break; 
        }
    }

    if (db) (void)ip6_pool_sync_with_leases(pool, db);

   for (uint16_t i = 0; i < subnet->host_count; ++i) {
        const dhcpv6_static_host_t* h = &subnet->hosts[i];
        if (!h->has_fixed_address6_bin) continue;

        
        (void)ip6_pool_reserve_ip(pool, h->fixed_addr6_bin,
                                  h->duid[0] ? h->duid : NULL);
    }

    log_info("ip6_pool_init: size=%u available=%u allocated=%u reserved=%u",
             pool->pool_size, pool->available_count,
             pool->allocated_count, pool->reserved_count);
    return 0;
}

void ip6_pool_free(struct ip6_pool_t* pool)
{
    if (!pool) return;
    memset(pool, 0, sizeof(*pool));
}


int ip6_pool_update_from_lease(struct ip6_pool_t* pool, dhcpv6_lease_t* L)
{
    if (!pool || !L) return -1;
    struct ip6_pool_entry_t* e = ip6_pool_find_entry(pool, L->ip6_addr);
    if (!e) return 0; 

   
    switch (e->state) {
        case IP6_STATE_AVAILABLE:
            if (pool->available_count) pool->available_count--;
            break;
        case IP6_STATE_ALLOCATED:
            if (pool->allocated_count) pool->allocated_count--;
            break;
        case IP6_STATE_RESERVED:
            if (pool->reserved_count) pool->reserved_count--;
            break;
        default:
            break;
    }

    ip6_state_t ns = ip6_state_from_lease_state(L->state);
    e->state = ns;

   if (ns == IP6_STATE_ALLOCATED || ns == IP6_STATE_RESERVED) {
        if (L->duid_len > 0) {
            if (duid_bin_to_hex(L->duid, L->duid_len, e->duid, sizeof(e->duid)) < 0) {
                e->duid[0] = '\0';
            }
        } else {
            e->duid[0] = '\0';
        }
    } else {
        e->duid[0] = '\0';
    }

    switch (ns) {
        case IP6_STATE_AVAILABLE:
            pool->available_count++;
            break;
        case IP6_STATE_ALLOCATED:
            pool->allocated_count++;
            e->last_allocated = L->starts;
            break;
        case IP6_STATE_RESERVED:
            pool->reserved_count++;
            break;
        default:
            break;
    }

   
    return 0;
}

int ip6_pool_sync_with_leases(struct ip6_pool_t* pool, lease_v6_db_t* db)
{
    if (!pool || !db) return -1;
    for (uint32_t i = 0; i < db->count; ++i) {
        if (!db->leases[i].in_use) continue;
        (void)ip6_pool_update_from_lease(pool, &db->leases[i]);
    }
    return 0;
}



int ip6_pool_mark_conflict(struct ip6_pool_t* pool, struct in6_addr ip, lease_v6_db_t* db, const char* reason)
{
    if (!pool) return -1;
    struct ip6_pool_entry_t* e = ip6_pool_find_entry(pool, ip);
    if (!e) return -1;

    if (e->state == IP6_STATE_AVAILABLE && pool->available_count) pool->available_count--;
    if (e->state == IP6_STATE_ALLOCATED && pool->allocated_count) pool->allocated_count--;
    if (e->state == IP6_STATE_RESERVED  && pool->reserved_count)  pool->reserved_count--;

    e->state = IP6_STATE_CONFLICT;

    if (db) (void)lease_v6_mark_conflict(db, &ip, reason ? reason : "probe");
    return 0;
}

int ip6_pool_release_ip(struct ip6_pool_t* pool, struct in6_addr ip, lease_v6_db_t* db)
{
    if (!pool) return -1;
    struct ip6_pool_entry_t* e = ip6_pool_find_entry(pool, ip);
    if (!e) return -1;

    if (e->state == IP6_STATE_ALLOCATED) {
        if (pool->allocated_count) pool->allocated_count--;
        pool->available_count++;
        e->state = IP6_STATE_AVAILABLE;
        e->duid[0] = '\0';
        e->last_allocated = 0;
    }

    if (db) (void)lease_v6_release_ip(db, &ip);
    return 0;
}



int ip6_pool_reserve_ip(struct ip6_pool_t* pool, struct in6_addr ip, const char* duid)
{
    if (!pool) return -1;
    struct ip6_pool_entry_t* e = ip6_pool_find_entry(pool, ip);
    if (!e) return -1;

    if (e->state == IP6_STATE_AVAILABLE && pool->available_count) pool->available_count--;
    if (e->state == IP6_STATE_ALLOCATED && pool->allocated_count) pool->allocated_count--;
    if (e->state == IP6_STATE_RESERVED  && pool->reserved_count)  pool->reserved_count--;

    e->state = IP6_STATE_RESERVED;
    e->last_allocated = time(NULL);

    if (duid && *duid) {
        e->duid[0] = '\0';
        strncpy(e->duid, duid, sizeof(e->duid)-1);
    }

    pool->reserved_count++;
    return 0;
}



struct ip6_allocation_result_t
ip6_pool_allocate(struct ip6_pool_t* pool,
                  const char* duid,
                  uint16_t duid_len,
                  uint32_t iaid,
                  const char* hostname_opt,
                  struct in6_addr requested_ip,
                  dhcpv6_config_t* config,
                  lease_v6_db_t* db,
                  uint32_t lease_time)
{
    struct ip6_allocation_result_t R = {0};

    if (!pool || !db || !duid || !config) {
        snprintf(R.error_message, sizeof(R.error_message), "invalid params");
        return R;
    }

    bool do_probe=false;
    uint32_t tmo=0;
    get_effective_probe(config, pool->subnet, &do_probe, &tmo);

  
    for (uint16_t i = 0; i < pool->subnet->host_count; ++i) {
        const dhcpv6_static_host_t* h = &pool->subnet->hosts[i];
        if (!h->has_fixed_address6_bin) continue;
        if (h->duid[0] && strcmp(h->duid, duid) == 0) {
            struct ip6_pool_entry_t* e = ip6_pool_find_entry(pool, h->fixed_addr6_bin);
            if (!e) break;

            if (do_probe && ip6_ping_check(e->ip_address, tmo)) {
                R.err_is_conflict = true; R.conflict_ip = e->ip_address; R.conflict_reason = "icmp6 echo reply";
                (void)ip6_pool_mark_conflict(pool, e->ip_address, db, R.conflict_reason);
                snprintf(R.error_message, sizeof(R.error_message), "conflict on reserved address");
                return R;
            }

           
            if (e->state == IP6_STATE_AVAILABLE && pool->available_count) pool->available_count--;
            if (e->state == IP6_STATE_RESERVED  && pool->reserved_count)  pool->reserved_count--;
            if (e->state != IP6_STATE_ALLOCATED) pool->allocated_count++;
            e->state = IP6_STATE_ALLOCATED;
            e->last_allocated = time(NULL);
            strncpy(e->duid, duid, sizeof(e->duid)-1);

          
            if (!lease_v6_add_ia_na(db, duid, duid_len, iaid, &e->ip_address, lease_time, hostname_opt)) {
              
                e->state = IP6_STATE_AVAILABLE;
                e->duid[0] = '\0';
                if (pool->allocated_count) pool->allocated_count--;
                pool->available_count++;
                snprintf(R.error_message, sizeof(R.error_message), "lease persist failed");
                return R;
            }
            R.success = true; R.ip_address = e->ip_address;
            return R;
        }
    }

   
    // Check for existing allocation for this DUID
    for (uint32_t i = 0; i < pool->pool_size; ++i) {
        struct ip6_pool_entry_t* e = &pool->entries[i];
        if (e->state == IP6_STATE_ALLOCATED && e->duid[0] && strcmp(e->duid, duid) == 0) {
             if (!lease_v6_add_ia_na(db, duid, duid_len, iaid, &e->ip_address, lease_time, hostname_opt)) {
                    snprintf(R.error_message, sizeof(R.error_message), "lease refresh failed");
             }
            R.success = true; 
            R.is_new = false; 
            R.ip_address = e->ip_address; 
            return R;
        }
    }

   
    if (!IN6_IS_ADDR_UNSPECIFIED(&requested_ip)) {
        if (ip6_pool_is_in_range(pool, requested_ip) && ip6_pool_is_available(pool, requested_ip)) {
            if (do_probe && ip6_ping_check(requested_ip, tmo)) {
                R.err_is_conflict=true; R.conflict_ip=requested_ip; R.conflict_reason="icmp6 echo reply";
                (void)ip6_pool_mark_conflict(pool, requested_ip, db, R.conflict_reason);
                snprintf(R.error_message, sizeof(R.error_message), "conflict on requested address");
                return R;
            }

            struct ip6_pool_entry_t* e = ip6_pool_find_entry(pool, requested_ip);
            if (e) {
                
                if (e->state == IP6_STATE_AVAILABLE && pool->available_count) pool->available_count--;
                if (e->state == IP6_STATE_RESERVED  && pool->reserved_count)  pool->reserved_count--;
                pool->allocated_count++;
                e->state = IP6_STATE_ALLOCATED;
                e->last_allocated = time(NULL);
                strncpy(e->duid, duid, sizeof(e->duid)-1);

                if (!lease_v6_add_ia_na(db, duid, duid_len, iaid, &e->ip_address, lease_time, hostname_opt)) {
                    e->state = IP6_STATE_AVAILABLE;
                    e->duid[0] = '\0';
                    if (pool->allocated_count) pool->allocated_count--;
                    pool->available_count++;
                    snprintf(R.error_message, sizeof(R.error_message), "lease persist failed");
                    return R;
                }
                R.success = true; 
                R.is_new = true;
                R.ip_address = e->ip_address; return R;
            }
        }
    }

    for (uint32_t i = 0; i < pool->pool_size; ++i) {
        struct ip6_pool_entry_t* e = &pool->entries[i];
        if (e->state != IP6_STATE_AVAILABLE) continue;

        if (do_probe && ip6_ping_check(e->ip_address, tmo)) {
            (void)ip6_pool_mark_conflict(pool, e->ip_address, db, "icmp6 echo reply");
            continue;
        }

        if (pool->available_count) pool->available_count--;
        pool->allocated_count++;
        e->state = IP6_STATE_ALLOCATED;
        e->last_allocated = time(NULL);
        strncpy(e->duid, duid, sizeof(e->duid)-1);

        if (!lease_v6_add_ia_na(db, duid, duid_len, iaid, &e->ip_address, lease_time, hostname_opt)) {
            e->state = IP6_STATE_AVAILABLE;
            e->duid[0] = '\0';
            if (pool->allocated_count) pool->allocated_count--;
            pool->available_count++;
            snprintf(R.error_message, sizeof(R.error_message), "lease persist failed");
            continue;
        }
        R.success = true;
        R.is_new = true;
        R.ip_address = e->ip_address; 
        goto end_and_save;
    }
    
    snprintf(R.error_message, sizeof(R.error_message), "no free addresses");

end_and_save:
    return R;
}


/* -------- print -------- */

void ip6_pool_print_stats(const struct ip6_pool_t* pool)
{
    if (!pool) return;
    char pfx[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &pool->subnet->prefix_bin, pfx, sizeof(pfx));
    printf("\n--- IPv6 Pool Statistics ---\n");
    printf("Subnet: %s/%u\n", pfx, pool->subnet->prefix_len);
    printf("Pool Size: %u\n", pool->pool_size);
    printf("Available: %u\n", pool->available_count);
    printf("Allocated: %u\n", pool->allocated_count);
    printf("Reserved:  %u\n", pool->reserved_count);
    printf("Utilization: %.1f%%\n",
           pool->pool_size ? (pool->allocated_count * 100.0 / pool->pool_size) : 0.0);
}

void ip6_pool_print_detailed(const struct ip6_pool_t* pool)
{
    if (!pool) return;
    ip6_pool_print_stats(pool);

    printf("\n--- IPv6 Pool Entries ---\n");
    for (uint32_t i = 0; i < pool->pool_size; ++i) {
        const struct ip6_pool_entry_t* e = &pool->entries[i];
        char ip[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &e->ip_address, ip, sizeof(ip));
        printf("%s - %s", ip, ip6_state_to_string(e->state));
        if (e->state == IP6_STATE_ALLOCATED || e->state == IP6_STATE_RESERVED) {
            if (e->duid[0]) printf(" - DUID: %s", e->duid);
        }
        printf("\n");
    }
}