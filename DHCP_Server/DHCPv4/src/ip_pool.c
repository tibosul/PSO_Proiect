#define _GNU_SOURCE
#include <asm-generic/errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../include/src/config_v4.h"
#include "../include/src/ip_pool.h"
#include "../include/src/lease_v4.h"
#include <sys/time.h>

const char *ip_state_to_string(ip_state_t state)
{
    switch (state)
    {
    case IP_STATE_AVAILABLE:
        return "available";
    case IP_STATE_ALLOCATED:
        return "allocated";
    case IP_STATE_RESERVED:
        return "reserved";
    case IP_STATE_EXCLUDED:
        return "excluded";
    case IP_STATE_CONFLICT:
        return "conflict";
    default:
        return "unknown";
    }
}

ip_state_t ip_state_from_string(const char *str)
{
    if (strcmp(str, "available") == 0)
        return IP_STATE_AVAILABLE;
    if (strcmp(str, "allocated") == 0)
        return IP_STATE_ALLOCATED;
    if (strcmp(str, "reserved") == 0)
        return IP_STATE_RESERVED;
    if (strcmp(str, "excluded") == 0)
        return IP_STATE_EXCLUDED;
    if (strcmp(str, "conflict") == 0)
        return IP_STATE_CONFLICT;

    return IP_STATE_UNKNOWN;
}

// Map lease state to IP pool state
ip_state_t ip_state_from_lease_state(lease_state_t lease_state)
{
    switch (lease_state)
    {
    case LEASE_STATE_ACTIVE:
        return IP_STATE_ALLOCATED;
    case LEASE_STATE_RESERVED:
        return IP_STATE_RESERVED;
    case LEASE_STATE_ABANDONED:
        return IP_STATE_CONFLICT;
    case LEASE_STATE_FREE:
    case LEASE_STATE_EXPIRED:
    case LEASE_STATE_RELEASED:
        return IP_STATE_AVAILABLE;
    case LEASE_STATE_BACKUP:
        return IP_STATE_ALLOCATED; // Backup leases are still allocated
    default:
        return IP_STATE_UNKNOWN;
    }
}

bool ip_is_network_address(struct in_addr ip, struct in_addr network, struct in_addr netmask)
{
    uint32_t ip_val = ntohl(ip.s_addr);
    uint32_t net_val = ntohl(network.s_addr);
    uint32_t mask_val = ntohl(netmask.s_addr);

    return ((ip_val & mask_val) == net_val) && (ip_val == net_val);
}

bool ip_is_broadcast_address(struct in_addr ip, struct in_addr network, struct in_addr netmask)
{
    uint32_t ip_val = ntohl(ip.s_addr);
    uint32_t net_val = ntohl(network.s_addr);
    uint32_t mask_val = ntohl(netmask.s_addr);
    uint32_t broadcast_val = net_val | (~mask_val);

    return ip_val == broadcast_val;
}

bool ip_is_gateway(struct in_addr ip, struct in_addr gateway)
{
    return ip.s_addr == gateway.s_addr;
}

struct ip_pool_entry_t *ip_pool_find_entry(struct ip_pool_t *pool, struct in_addr ip)
{
    if (!pool)
        return NULL;

    for (uint32_t i = 0; i < pool->pool_size; i++)
    {
        if (pool->entries[i].ip_address.s_addr == ip.s_addr)
        {
            return &pool->entries[i];
        }
    }
    return NULL;
}

bool ip_pool_is_in_range(struct ip_pool_t *pool, struct in_addr ip)
{
    if (!pool || !pool->subnet)
        return false;

    uint32_t ip_val = ntohl(ip.s_addr);
    uint32_t start_val = ntohl(pool->subnet->range_start.s_addr);
    uint32_t end_val = ntohl(pool->subnet->range_end.s_addr);

    return (ip_val >= start_val && ip_val <= end_val);
}

bool ip_pool_is_available(struct ip_pool_t *pool, struct in_addr ip)
{
    struct ip_pool_entry_t *entry = ip_pool_find_entry(pool, ip);
    if (!entry)
        return false;

    return entry->state == IP_STATE_AVAILABLE;
}

static uint16_t icmp_checksum(void *data, int len)
{
    uint16_t *buf = (uint16_t *)data;
    uint32_t sum = 0;

    for (; len > 1; len -= 2)
    {
        sum += *buf++;
    }

    if (len == 1)
    {
        sum += *(uint8_t *)buf;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);

    return ~sum;
}

bool ip_ping_check(struct in_addr ip, uint32_t timeout_ms)
{
    int sockfd;
    struct sockaddr_in addr;
    struct icmphdr icmp_hdr;
    uint8_t packet[64];
    struct timeval tv;
    fd_set readfds;

    // Create raw socket (requires root/CAP_NET_RAW)
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0)
    {
        // If we can't create raw socket, assume IP is available
        // (ping check requires elevated privileges)
        return false;
    }

    // Set timeout
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    // Prepare destination
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr = ip;

    // Prepare ICMP echo request
    memset(&icmp_hdr, 0, sizeof(icmp_hdr));
    icmp_hdr.type = ICMP_ECHO;
    icmp_hdr.code = 0;
    icmp_hdr.un.echo.id = getpid();
    icmp_hdr.un.echo.sequence = 1;

    memcpy(packet, &icmp_hdr, sizeof(icmp_hdr));
    icmp_hdr.checksum = icmp_checksum(packet, sizeof(icmp_hdr));
    memcpy(packet, &icmp_hdr, sizeof(icmp_hdr));

    if (sendto(sockfd, packet, sizeof(icmp_hdr), 0, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(sockfd);
        return false;
    }

    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    int result = select(sockfd + 1, &readfds, NULL, NULL, &tv);
    close(sockfd);

    return (result > 0);
}

int ip_pool_init(struct ip_pool_t *pool, struct dhcp_subnet_t *subnet, struct lease_database_t *lease_db)
{
    if (!pool || !subnet)
        return -1;

    memset(pool, 0, sizeof(struct ip_pool_t));

    // Initialize mutex
    if (pthread_mutex_init(&pool->mutex, NULL) != 0)
    {
        perror("Failed to initialize IP pool mutex");
        return -1;
    }

    pool->subnet = subnet;

    uint32_t start_ip = ntohl(subnet->range_start.s_addr);
    uint32_t end_ip = ntohl(subnet->range_end.s_addr);

    pool->pool_size = 0;
    for (uint32_t ip_val = start_ip;
    ip_val <= end_ip && pool->pool_size < MAX_POOL_SIZE; ip_val++)
    {
        struct ip_pool_entry_t *entry = &pool->entries[pool->pool_size];
        entry->ip_address.s_addr = htonl(ip_val);
        entry->state = IP_STATE_AVAILABLE;
        memset(entry->mac_address, 0, 6);
        entry->last_allocated = 0;
        entry->lease_id = 0; // Initialize lease reference (0 = no lease)

        if (ip_is_network_address(entry->ip_address, subnet->network, subnet->netmask))
        {
            entry->state = IP_STATE_EXCLUDED;
        }
        else if (ip_is_broadcast_address(entry->ip_address, subnet->network, subnet->netmask))
        {
            entry->state = IP_STATE_EXCLUDED;
        }
        else if (ip_is_gateway(entry->ip_address, subnet->router))
        {
            entry->state = IP_STATE_EXCLUDED;
        }
        else
        {
            pool->available_count++;
        }
        pool->pool_size++;
    }

    for (uint32_t i = 0; i < subnet->host_count; i++)
    {
        struct dhcp_host_reservation_t *host = &subnet->hosts[i];
        struct ip_pool_entry_t *entry =
        ip_pool_find_entry(pool, host->fixed_address);

        if (entry)
        {
            entry->state = IP_STATE_RESERVED;
            memcpy(entry->mac_address, host->mac_address, 6);
            pool->available_count--;
        }
    }

    // Sync with lease database - handle all lease states
    if (lease_db)
    {
        time_t now = time(NULL);
        for (uint32_t i = 0; i < lease_db->lease_count; i++)
        {
            struct dhcp_lease_t *lease = &lease_db->leases[i];

            // Check if lease is expired and update state if needed
            if (lease->state == LEASE_STATE_ACTIVE && lease->end_time < now)
            {
                lease->state = LEASE_STATE_EXPIRED;
            }

            struct ip_pool_entry_t *entry =
            ip_pool_find_entry(pool, lease->ip_address);
            if (!entry)
                continue; // IP not in this pool's range

            // Skip if already marked as RESERVED by config (static reservations take
            // priority)
            if (entry->state == IP_STATE_RESERVED)
                continue;

            // Map lease state to IP state
            ip_state_t new_state = ip_state_from_lease_state(lease->state);

            // Update pool entry based on lease state
            if (new_state == IP_STATE_ALLOCATED)
            {
                if (entry->state == IP_STATE_AVAILABLE)
                {
                    pool->available_count--;
                    pool->allocated_count++;
                }
                entry->state = IP_STATE_ALLOCATED;
                memcpy(entry->mac_address, lease->mac_address, 6);
                entry->last_allocated = lease->start_time;
                entry->lease_id = lease->lease_id;
            }
            else if (new_state == IP_STATE_CONFLICT)
            {
                if (entry->state == IP_STATE_AVAILABLE)
                {
                    pool->available_count--;
                }
                entry->state = IP_STATE_CONFLICT;
                entry->lease_id = lease->lease_id;
            }
            else if (new_state == IP_STATE_AVAILABLE)
            {
                // Lease is expired/released/free - make IP available if not already
                if (entry->state == IP_STATE_ALLOCATED)
                {
                    pool->allocated_count--;
                    pool->available_count++;
                }
                entry->state = IP_STATE_AVAILABLE;
                memset(entry->mac_address, 0, 6);
                entry->lease_id = lease->lease_id;
            }
        }
    }

    return 0;
}

void ip_pool_free(struct ip_pool_t *pool)
{
    if (pool)
    {
        pthread_mutex_destroy(&pool->mutex);
        memset(pool, 0, sizeof(struct ip_pool_t));
    }
}

int ip_pool_reserve_ip(struct ip_pool_t *pool, struct in_addr ip, const uint8_t mac[6])
{
    if (!pool || !mac)
        return -1;

    pthread_mutex_lock(&pool->mutex);
    struct ip_pool_entry_t *entry = ip_pool_find_entry(pool, ip);
    if (!entry)
    {
        pthread_mutex_unlock(&pool->mutex);
        return -1;
    }

    // Check if already allocated to DIFFERENT MAC
    if (entry->state == IP_STATE_ALLOCATED && memcmp(entry->mac_address, mac, 6) != 0)
    {
        // IP already allocated to different client
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip, ip_str, INET_ADDRSTRLEN);
        fprintf(stderr, "WARNING: IP %s already allocated to different MAC\n", ip_str);
        pthread_mutex_unlock(&pool->mutex);
        return -1;
    }

    // Don't allow reserving static reservations or excluded IPs
    // Don't allow reserving static reservations or excluded IPs
    if (entry->state == IP_STATE_RESERVED || entry->state == IP_STATE_EXCLUDED)
    {
        pthread_mutex_unlock(&pool->mutex);
        return -1;
    }

    // If already allocated to SAME MAC, just update timestamp (idempotent)
    if (entry->state == IP_STATE_ALLOCATED && memcmp(entry->mac_address, mac, 6) == 0)
    {
        entry->last_allocated = time(NULL);
        pthread_mutex_unlock(&pool->mutex);
        return 0; // No counter change needed
    }

    // Adjust counts based on state transition
    ip_state_t old_state = entry->state;

    if (old_state == IP_STATE_AVAILABLE)
    {
        pool->available_count--;
        pool->allocated_count++;
    }
    else if (old_state == IP_STATE_CONFLICT)
    {
        // Moving from conflict to allocated
        pool->allocated_count++;
    }
    // If already ALLOCATED, counts don't change (just updating MAC/time)

    entry->state = IP_STATE_ALLOCATED;
    memcpy(entry->mac_address, mac, 6);
    entry->last_allocated = time(NULL);

    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

int ip_pool_release_ip(struct ip_pool_t *pool, struct in_addr ip)
{
    if (!pool)
        return -1;

    pthread_mutex_lock(&pool->mutex);
    struct ip_pool_entry_t *entry = ip_pool_find_entry(pool, ip);
    if (!entry)
    {
        pthread_mutex_unlock(&pool->mutex);
        return -1;
    }

    if (entry->state == IP_STATE_ALLOCATED)
    {
        entry->state = IP_STATE_AVAILABLE;
        memset(entry->mac_address, 0, 6);
        pool->allocated_count--;
        pool->available_count++;
    }

    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

int ip_pool_mark_conflict(struct ip_pool_t *pool, struct in_addr ip)
{
    if (!pool)
        return -1;

    pthread_mutex_lock(&pool->mutex);
    struct ip_pool_entry_t *entry = ip_pool_find_entry(pool, ip);
    if (!entry)
    {
        pthread_mutex_unlock(&pool->mutex);
        return -1;
    }

    // Update counters based on previous state
    if (entry->state == IP_STATE_AVAILABLE)
    {
        pool->available_count--;
    }
    else if (entry->state == IP_STATE_ALLOCATED)
    {
        pool->allocated_count--;
    }
    // EXCLUDED and RESERVED don't affect counters

    entry->state = IP_STATE_CONFLICT;
    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

struct ip_allocation_result_t ip_pool_allocate(struct ip_pool_t *pool, const uint8_t mac[6],
                                               struct in_addr requested_ip, struct dhcp_config_t *config)
{
    struct ip_allocation_result_t result = {0};

    if (!pool || !mac || !config)
    {
        result.success = false;
        snprintf(result.error_message, sizeof(result.error_message), "Invalid parameters");
        return result;
    }

    pthread_mutex_lock(&pool->mutex);

    // Priority 1: check for static reservation
    for (uint32_t i = 0; i < pool->subnet->host_count; i++)
    {
        struct dhcp_host_reservation_t *host = &pool->subnet->hosts[i];

        if (memcmp(host->mac_address, mac, 6) == 0)
        {
            result.success = true;
            result.ip_address = host->fixed_address;
            pthread_mutex_unlock(&pool->mutex);
            return result;
        }
    }

    // Priority 2: check if client already has an allocated IP
    for (uint32_t i = 0; i < pool->pool_size; i++)
    {
        struct ip_pool_entry_t *entry = &pool->entries[i];

        if (entry->state == IP_STATE_ALLOCATED && memcmp(entry->mac_address, mac, 6) == 0)
        {
            result.success = true;
            result.ip_address = entry->ip_address;
            pthread_mutex_unlock(&pool->mutex);
            return result;
        }
    }

    // Priority 3: If client requested a specific IP, try to honor it
    if (requested_ip.s_addr != 0)
    {
        if (ip_pool_is_in_range(pool, requested_ip) && ip_pool_is_available(pool, requested_ip))
        {
            // Ping check if enabled
            if (config->global.ping_check)
            {
                if (ip_ping_check(requested_ip, config->global.ping_timeout * 1000))
                {
                    ip_pool_mark_conflict(pool, requested_ip);
                }
                else
                {
                    result.success = true;
                    result.ip_address = requested_ip;
                    // Note: ip_pool_reserve_ip locks internally, so we unlock here first?
                    // No, ip_pool_reserve_ip is called from outside usually.
                    // If we call it from here, we have a recursive lock problem unless we
                    // use a recursive mutex or factor out the logic. Let's implement the
                    // reservation logic directly here to avoid recursive lock issues.

                    struct ip_pool_entry_t *entry = ip_pool_find_entry(pool, requested_ip);
                    if (entry)
                    {
                        if (entry->state == IP_STATE_AVAILABLE)
                            pool->available_count--;
                        entry->state = IP_STATE_ALLOCATED;
                        memcpy(entry->mac_address, mac, 6);
                        entry->last_allocated = time(NULL);
                        pool->allocated_count++;
                    }

                    pthread_mutex_unlock(&pool->mutex);
                    return result;
                }
            }
            else
            {
                result.success = true;
                result.ip_address = requested_ip;

                struct ip_pool_entry_t *entry = ip_pool_find_entry(pool, requested_ip);
                if (entry)
                {
                    if (entry->state == IP_STATE_AVAILABLE)
                        pool->available_count--;
                    entry->state = IP_STATE_ALLOCATED;
                    memcpy(entry->mac_address, mac, 6);
                    entry->last_allocated = time(NULL);
                    pool->allocated_count++;
                }

                pthread_mutex_unlock(&pool->mutex);
                return result;
            }
        }
    }

    // Priority 4: Find first available IP
    for (uint32_t i = 0; i < pool->pool_size; i++)
    {
        struct ip_pool_entry_t *entry = &pool->entries[i];

        if (entry->state == IP_STATE_AVAILABLE)
        {
            // Ping check if enabled
            if (config->global.ping_check)
            {
                if (ip_ping_check(entry->ip_address, config->global.ping_timeout * 1000))
                {
                    ip_pool_mark_conflict(pool, entry->ip_address);
                    continue;
                }
            }

            result.success = true;
            result.ip_address = entry->ip_address;

            // Inline reservation logic
            pool->available_count--;
            entry->state = IP_STATE_ALLOCATED;
            memcpy(entry->mac_address, mac, 6);
            entry->last_allocated = time(NULL);
            pool->allocated_count++;

            pthread_mutex_unlock(&pool->mutex);
            return result;
        }
    }

    // No available IPs
    // No available IPs
    result.success = false;
    snprintf(result.error_message, sizeof(result.error_message), "No available IPs in pool");
    pthread_mutex_unlock(&pool->mutex);
    return result;
}

// Update a single pool entry from a lease
int ip_pool_update_from_lease(struct ip_pool_t *pool, struct dhcp_lease_t *lease)
{
    if (!pool || !lease)
        return -1;

    struct ip_pool_entry_t *entry = ip_pool_find_entry(pool, lease->ip_address);
    if (!entry)
        return -1; // IP not in this pool's range

    // Skip if this is a static reservation (config takes priority)
    if (entry->state == IP_STATE_RESERVED)
        return 0;

    // Check if lease is expired
    time_t now = time(NULL);
    if (lease->state == LEASE_STATE_ACTIVE && lease->end_time < now)
    {
        lease->state = LEASE_STATE_EXPIRED;
    }

    // Map lease state to IP state
    ip_state_t old_state = entry->state;
    ip_state_t new_state = ip_state_from_lease_state(lease->state);

    // Handle ALL state transitions by decrement-then-increment
    // Decrement old state counter
    if (old_state == IP_STATE_AVAILABLE)
    {
        pool->available_count--;
    }
    else if (old_state == IP_STATE_ALLOCATED)
    {
        pool->allocated_count--;
    }
    // CONFLICT, EXCLUDED, RESERVED don't have dedicated counters

    // Increment new state counter
    if (new_state == IP_STATE_AVAILABLE)
    {
        pool->available_count++;
    }
    else if (new_state == IP_STATE_ALLOCATED)
    {
        pool->allocated_count++;
    }
    // CONFLICT, EXCLUDED, RESERVED don't have dedicated counters

    // Update entry
    entry->state = new_state;
    entry->lease_id = lease->lease_id;

    if (new_state == IP_STATE_ALLOCATED)
    {
        memcpy(entry->mac_address, lease->mac_address, 6);
        entry->last_allocated = lease->start_time;
    }
    else if (new_state == IP_STATE_AVAILABLE)
    {
        memset(entry->mac_address, 0, 6);
    }

    return 0;
}

// Sync entire pool with lease database (useful after lease changes)
int ip_pool_sync_with_leases(struct ip_pool_t *pool, struct lease_database_t *lease_db)
{
    if (!pool || !lease_db)
        return -1;

    pthread_mutex_lock(&pool->mutex);

    for (uint32_t i = 0; i < lease_db->lease_count; i++)
    {
        ip_pool_update_from_lease(pool, &lease_db->leases[i]);
    }

    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

// Allocate IP and create corresponding lease in database
struct dhcp_lease_t *ip_pool_allocate_and_create_lease(struct ip_pool_t *pool, struct lease_database_t *lease_db,
                                                       const uint8_t mac[6], struct in_addr requested_ip,
                                                       struct dhcp_config_t *config, uint32_t lease_time)
{
    if (!pool || !lease_db || !mac || !config)
        return NULL;

    // First, try to allocate from pool
    struct ip_allocation_result_t result = ip_pool_allocate(pool, mac, requested_ip, config);

    if (!result.success)
    {
        return NULL;
    }

    // Check if lease already exists for this IP
    struct dhcp_lease_t *existing_lease = lease_db_find_by_ip(lease_db, result.ip_address);

    if (existing_lease)
    {
        // Update existing lease
        if (lease_db_renew_lease(lease_db, result.ip_address, lease_time) == 0)
        {
            // Update pool entry reference
            struct ip_pool_entry_t *entry = ip_pool_find_entry(pool, result.ip_address);
            if (entry)
            {
                entry->lease_id = existing_lease->lease_id;
            }
            return existing_lease;
        }
        else
        {
            // Rollback: release IP from pool
            ip_pool_release_ip(pool, result.ip_address);
            return NULL;
        }
    }
    else
    {
        // Create new lease
        struct dhcp_lease_t *new_lease = lease_db_add_lease(lease_db, result.ip_address, mac, lease_time);

        if (new_lease)
        {
            // Update pool entry reference
            struct ip_pool_entry_t *entry = ip_pool_find_entry(pool, result.ip_address);
            if (entry)
            {
                entry->lease_id = new_lease->lease_id;
            }
            return new_lease;
        }
        else
        {
            // Rollback: release IP from pool
            ip_pool_release_ip(pool, result.ip_address);
            return NULL;
        }
    }
}

void ip_pool_print_stats(const struct ip_pool_t *pool)
{
    if (!pool)
        return;

    char network_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &pool->subnet->network, network_str, INET_ADDRSTRLEN);

    printf("\n--- IP Pool Statistics ---\n");
    printf("Subnet: %s\n", network_str);
    printf("Pool Size: %u\n", pool->pool_size);
    printf("Available: %u\n", pool->available_count);
    printf("Allocated: %u\n", pool->allocated_count);
    printf("Utilization: %.1f%%\n", pool->pool_size > 0 ? (pool->allocated_count * 100.0 / pool->pool_size): 0.0);
}

void ip_pool_print_detailed(const struct ip_pool_t *pool)
{
    if (!pool)
        return;

    ip_pool_print_stats(pool);

    printf("\n--- IP Pool Entries ---\n");
    for (uint32_t i = 0; i < pool->pool_size; i++)
    {
        const struct ip_pool_entry_t *entry = &pool->entries[i];
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &entry->ip_address, ip_str, INET_ADDRSTRLEN);

        printf("%s - %s", ip_str, ip_state_to_string(entry->state));

        if (entry->state == IP_STATE_ALLOCATED || entry->state == IP_STATE_RESERVED)
        {
            printf(" - MAC: %02x:%02x:%02x:%02x:%02x:%02x", entry->mac_address[0],
            entry->mac_address[1], entry->mac_address[2],
            entry->mac_address[3], entry->mac_address[4],
            entry->mac_address[5]);
        }

        printf("\n");
    }
}