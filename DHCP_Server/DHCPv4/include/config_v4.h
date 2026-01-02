#ifndef CONFIG_V4_H
#define CONFIG_V4_H

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>

#define MAX_SUBNETS 32
#define MAX_HOSTS_PER_SUBNET 128
#define MAX_DNS_SERVERS 4
#define MAX_NTP_SERVERS 4
#define MAX_HOSTNAME_LENGTH 256
#define MAX_DOMAIN_LENGTH 256

struct dhcp_global_options_t
{
    bool authoritative;
    uint32_t default_lease_time; // in seconds
    uint32_t max_lease_time;     // in seconds
    struct in_addr dns_servers[MAX_DNS_SERVERS];
    uint32_t dns_server_count;  // number of DNS servers
    bool ping_check;            // whether to ping the client before giving an address
    uint32_t ping_timeout;      // in seconds
    char ddns_update_style[32]; // DHCP or NONE
};

struct dhcp_host_reservation_t
{
    char name[MAX_HOSTNAME_LENGTH];
    uint8_t mac_address[6];
    struct in_addr fixed_address;
    char hostname[MAX_HOSTNAME_LENGTH];
};

struct dhcp_subnet_t
{
    struct in_addr network;
    struct in_addr netmask;
    struct in_addr range_start;
    struct in_addr range_end;

    // Subnet specific options
    struct in_addr router;
    struct in_addr broadcast;
    struct in_addr subnet_mask;
    char domain_name[MAX_DOMAIN_LENGTH];
    struct in_addr dns_servers[MAX_DNS_SERVERS];
    uint32_t dns_server_count;
    int32_t time_offset;
    struct in_addr ntp_servers[MAX_NTP_SERVERS];
    uint32_t ntp_server_count;
    struct in_addr netbios_servers[MAX_NTP_SERVERS];
    uint32_t netbios_server_count;

    // Subnet-specific lease times (optional overrides)
    uint32_t default_lease_time; // 0 means use global
    uint32_t max_lease_time;     // 0 means use global

    // Host reservation
    struct dhcp_host_reservation_t hosts[MAX_HOSTS_PER_SUBNET];
    uint32_t host_count;
};

struct dhcp_config_t
{
    struct dhcp_global_options_t global;
    struct dhcp_subnet_t subnets[MAX_SUBNETS];
    uint32_t subnet_count;
};

int parse_config_file(const char *filename, struct dhcp_config_t *config);
void free_config(struct dhcp_config_t *config);
void print_config(const struct dhcp_config_t *config);

int parse_ip_address(const char *str, struct in_addr *addr);
int parse_mac_address(const char *str, uint8_t mac[6]);
struct dhcp_subnet_t *find_subnet_for_ip(struct dhcp_config_t *config, struct in_addr ip);
struct dhcp_host_reservation_t *find_host_by_mac(struct dhcp_subnet_t *subnet, const uint8_t mac[6]);

#endif // CONFIG_V4_H