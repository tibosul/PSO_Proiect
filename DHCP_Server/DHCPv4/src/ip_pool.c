#define _GNU_SOURCE
#include <asm-generic/errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
//#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip_icmp.h>
#include <unistd.h>
//#include <errno.h>
#include <sys/time.h>
#include "../include/ip_pool.h"
#include "../include/config_v4.h"
#include "../include/lease_v4.h"

const char* ip_state_to_string( ip_state_t state)
{
    switch(state)
    {
        case IP_STATE_AVAILABLE: return "available";
        case IP_STATE_ALLOCATED: return "allocated";
        case IP_STATE_RESERVED: return "reserved";
        case IP_STATE_EXCLUDED: return "excluded";
        case IP_STATE_CONFLICT: return "conflict";
        default: return "unknown";
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

struct ip_pool_entry_t* ip_pool_find_entry(struct ip_pool_t* pool, struct in_addr ip)
{
    if(!pool) return NULL;

    for(uint32_t i = 0; i < pool->pool_size; i++)
    {
        if(pool->entries[i].ip_address.s_addr == ip.s_addr)
        {
            return &pool->entries[i];
        }
    }
    return NULL;
}

bool ip_pool_is_in_range(struct ip_pool_t *pool, struct in_addr ip)
{
    if(!pool || !pool->subnet) return false;

    uint32_t ip_val = ntohl(ip.s_addr);
    uint32_t start_val = ntohl(pool->subnet->range_start.s_addr);
    uint32_t end_val = ntohl(pool->subnet->range_end.s_addr);

    return (ip_val >= start_val && ip_val <= end_val);
}

bool ip_pool_is_available(struct ip_pool_t *pool, struct in_addr ip)
{
    struct ip_pool_entry_t* entry = ip_pool_find_entry(pool, ip);
    if(!entry) return false;

    return entry->state == IP_STATE_AVAILABLE;
}

static uint16_t icmp_checksum(void* data, int len)
{
    uint16_t* buf = (uint16_t*)data;
    uint32_t sum = 0;

    for(; len > 1; len -= 2)
    {
        sum += *buf++;
    }

    if(len == 1)
    {
        sum += *(uint8_t*)buf;
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
    if(sockfd < 0)
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

    if(sendto(sockfd, packet, sizeof(icmp_hdr), 0, (struct sockaddr*)&addr, sizeof(addr)) < 0)
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
    if(!pool || !subnet) return -1;

    memset(pool, 0, sizeof(struct ip_pool_t));
    pool->subnet = subnet;

    uint32_t start_ip = ntohl(subnet->range_start.s_addr);
    uint32_t end_ip = ntohl(subnet->range_end.s_addr);

    pool->pool_size = 0;
    for(uint32_t ip_val = start_ip; ip_val <= end_ip && pool->pool_size < MAX_POOL_SIZE; ip_val++)
    {
        struct ip_pool_entry_t* entry = &pool->entries[pool->pool_size];
        entry->ip_address.s_addr = htonl(ip_val);
        entry->state = IP_STATE_AVAILABLE;
        memset(entry->mac_address, 0 ,6);
        entry->last_allocated = 0;

        if(ip_is_network_address(entry->ip_address, subnet->network,subnet->netmask))
        {
            entry->state = IP_STATE_EXCLUDED;
        }
        else if(ip_is_broadcast_address(entry->ip_address, subnet->network, subnet->netmask))
        {
            entry->state = IP_STATE_EXCLUDED;
        }
        else if(ip_is_gateway(entry->ip_address, subnet->router))
        {
            entry->state = IP_STATE_EXCLUDED;
        }
        else
        {
            pool->available_count++;
        }
        pool->pool_size++;
    }

    for(uint32_t i = 0; i < subnet->host_count; i++)
    {
        struct dhcp_host_reservation_t* host = &subnet->hosts[i];
        struct ip_pool_entry_t* entry = ip_pool_find_entry(pool, host->fixed_address);

        if(entry)
        {
            entry->state = IP_STATE_RESERVED;
            memcpy(entry->mac_address, host->mac_address, 6);
            pool->available_count--;
        }
    }

    if(lease_db)
    {
        for(uint32_t i = 0; i < lease_db->lease_count; i++)
        {
            struct dhcp_lease_t* lease = &lease_db->leases[i];

            if(lease->state == LEASE_STATE_ACTIVE)
            {
                struct ip_pool_entry_t* entry = ip_pool_find_entry(pool, lease->ip_address);

                if(entry && entry->state == IP_STATE_AVAILABLE)
                {
                    entry->state = IP_STATE_ALLOCATED;
                    memcpy(entry->mac_address, lease->mac_address, 6);
                    entry->last_allocated = lease->start_time;
                    pool->available_count--;
                    pool->allocated_count++;
                }
            }
        }
    }
    
    return 0;
}

void ip_pool_free(struct ip_pool_t *pool)
{
    if(pool)
    {
        memset(pool, 0, sizeof(struct ip_pool_t));
    }
}

int ip_pool_reserve_ip(struct ip_pool_t *pool, struct in_addr ip, const uint8_t mac[6])
{
    if(!pool || !mac) return -1;

    struct ip_pool_entry_t* entry = ip_pool_find_entry(pool, ip);
    if(!entry) return -1;

    // Adjust counts based on old state
    if(entry->state == IP_STATE_AVAILABLE)
    {
        pool->available_count--;
    }

    // Only increment allocated_count if it wasn't already ALLOCATED
    if(entry->state != IP_STATE_ALLOCATED)
    {
        pool->allocated_count++;
    }

    entry->state = IP_STATE_ALLOCATED;
    memcpy(entry->mac_address, mac, 6);
    entry->last_allocated = time(NULL);

    return 0;
}

int ip_pool_release_ip(struct ip_pool_t* pool, struct in_addr ip)
{
    if(!pool) return -1;

    struct ip_pool_entry_t* entry = ip_pool_find_entry(pool, ip);
    if(!entry) return -1;

    if(entry->state == IP_STATE_ALLOCATED)
    {
        entry->state = IP_STATE_AVAILABLE;
        memset(entry->mac_address, 0, 6);
        pool->allocated_count--;
        pool->available_count++;
    }

    return 0;
}

int ip_pool_mark_conflict(struct ip_pool_t *pool, struct in_addr ip)
{
    if(!pool) return -1;

    struct ip_pool_entry_t* entry = ip_pool_find_entry(pool, ip);
    if(!entry) return -1;

    if(entry->state == IP_STATE_AVAILABLE)
    {
        pool->available_count--;
    }

    entry->state = IP_STATE_CONFLICT;
    return 0;
}

struct ip_allocation_result_t ip_pool_allocate(struct ip_pool_t *pool, const uint8_t mac[6], struct in_addr requested_ip, struct dhcp_config_t *config)
{
    struct ip_allocation_result_t result = {0};

    if(!pool || !mac || !config)
    {
        result.success = false;
        snprintf(result.error_message, sizeof(result.error_message), "Invalid parameters");
        return result;
    }

    // Priority 1: check for static reservation
    for(uint32_t i = 0; i < pool->subnet->host_count; i++)
    {
        struct dhcp_host_reservation_t* host = &pool->subnet->hosts[i];

        if(memcmp(host->mac_address, mac, 6) == 0)
        {
            result.success = true;
            result.ip_address = host->fixed_address;
            return result;
        }
    }

    // Priority 2: check if client already has an allocated IP
    for(uint32_t i = 0; i < pool->pool_size; i++)
    {
        struct ip_pool_entry_t* entry = &pool->entries[i];

        if(entry->state == IP_STATE_ALLOCATED && memcmp(entry->mac_address, mac, 6) == 0)
        {
            result.success = true;
            result.ip_address = entry->ip_address;
            return result;
        }
    }

    // Priority 3: If client requested a specific IP, try to honor it
    if(requested_ip.s_addr != 0)
    {
        if(ip_pool_is_in_range(pool, requested_ip) && ip_pool_is_available(pool, requested_ip))
        {  
            // Ping check if enabled
            if(config->global.ping_check)
            {
                if(ip_ping_check(requested_ip, config->global.ping_timeout * 1000))
                {
                    ip_pool_mark_conflict(pool, requested_ip);
                }
                else
                {
                    result.success = true;
                    result.ip_address = requested_ip;
                    ip_pool_reserve_ip(pool, requested_ip, mac);
                    return result;
                }
            }
            else
            {
                result.success = true;
                result.ip_address = requested_ip;
                ip_pool_reserve_ip(pool, requested_ip, mac);
                return result;
            }
        }
    }

    // Priority 4: Find first available IP
    for(uint32_t i = 0; i < pool->pool_size; i++)
    {
        struct ip_pool_entry_t* entry = &pool->entries[i];

        if(entry->state == IP_STATE_AVAILABLE)
        {
            // Ping check if enabled
            if(config->global.ping_check)
            {
                if(ip_ping_check(entry->ip_address, config->global.ping_timeout * 1000))
                {
                    ip_pool_mark_conflict(pool, entry->ip_address);
                    continue;
                }
            }

            result.success = true;
            result.ip_address = entry->ip_address;
            ip_pool_reserve_ip(pool, entry->ip_address, mac);
            return result;
        }
    }

    // No available IPs
    result.success = false;
    snprintf(result.error_message, sizeof(result.error_message), "No available IPs in pool");
    return result;
}

void ip_pool_print_stats(const struct ip_pool_t *pool)
{
    if(!pool) return;

    char network_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &pool->subnet->network, network_str, INET_ADDRSTRLEN);

    printf("\n--- IP Pool Statistics ---\n");
    printf("Subnet: %s\n", network_str);
    printf("Pool Size: %u\n", pool->pool_size);
    printf("Available: %u\n", pool->available_count);
    printf("Allocated: %u\n", pool->allocated_count);
    printf("Utilization: %.1f%%\n", 
           pool->pool_size > 0 ? 
           (pool->allocated_count * 100.0 / pool->pool_size) : 0.0);
}

void ip_pool_print_detailed(const struct ip_pool_t *pool)
{
    if(!pool) return;
    
    ip_pool_print_stats(pool);
    
    printf("\n--- IP Pool Entries ---\n");
    for(uint32_t i = 0; i < pool->pool_size; i++)
    {
        const struct ip_pool_entry_t *entry = &pool->entries[i];
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &entry->ip_address, ip_str, INET_ADDRSTRLEN);
        
        printf("%s - %s", ip_str, ip_state_to_string(entry->state));
        
        if(entry->state == IP_STATE_ALLOCATED || entry->state == IP_STATE_RESERVED)
        {
            printf(" - MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                   entry->mac_address[0], entry->mac_address[1],
                   entry->mac_address[2], entry->mac_address[3],
                   entry->mac_address[4], entry->mac_address[5]);
        }
        
        printf("\n");
    }
}