#ifndef CONFIG_V6_H
#define CONFIG_V6_H

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>

#define HOSTNAME_MAX 256
#define Ip6_STR_MAX 80
#define DUID_MAX 130
#define MAX_SUBNET_V6 64
#define MAX_HOSTS_PER_SUBNET 200

typedef struct 
{
    char hostname[HOSTNAME_MAX];
    char duid[DUID_MAX];
    char fixed_address6[Ip6_STR_MAX];

    struct in6_addr fixed_addr6_bin;
    bool has_fixed_address6_bin; //if the fixed adress was valid and converted
}dhcpv6_static_host_t;

typedef struct 
{
    //Subnet details
   char prefix[Ip6_STR_MAX];
   uint8_t prefix_len;

   struct in6_addr prefix_bin;
   bool has_prefix_bin;

   char pool_start[Ip6_STR_MAX];
   char pool_end[Ip6_STR_MAX];

   struct in6_addr pool_start_bin;
   struct in6_addr pool_end_bin;
   bool has_pool_range; 

   char dns_servers[256];
   char domain_search[256];

   char sntp_servers[256];       
   bool has_sntp_servers;

   uint32_t info_refresh_time;   
   bool has_info_refresh_time;

   uint8_t preference;          
   bool has_preference;

   char sip_server_domain[256];
   bool has_sip_server_domain;

   char bootfile_url[256];
   bool has_bootfile_url;

   uint32_t default_lease_time;
   uint32_t max_lease_time;    

   dhcpv6_static_host_t hosts[MAX_HOSTS_PER_SUBNET];
   uint16_t host_count;

   //IA_PD
   bool pd_enabled;
   char pd_pool_start[Ip6_STR_MAX];
   char pd_pool_end[Ip6_STR_MAX];
   uint8_t pd_prefix_len;
   struct in6_addr pd_pool_start_bin;
   struct in6_addr pd_pool_end_bin;
   bool has_pd_pool;

   bool icmp6_probe_override;     
   bool icmp6_probe;              
   uint32_t icmp6_timeout_ms;         
   bool has_icmp6_timeout;
}dhcpv6_subnet_t;

typedef struct{
    uint32_t default_lease_time;
    uint32_t max_lease_time;

    char global_dns_servers[256];
    char global_domain_search[256];

    char sntp_servers[256];
    bool has_sntp_servers;

    uint32_t info_refresh_time;
    bool has_info_refresh_time;

    uint8_t preference;
    bool has_preference;

    char sip_server_domain[256];
    bool has_sip_server_domain;

    char bootfile_url[256];
    bool has_bootfile_url;

    bool icmp6_probe;              // enable/disable global ICMPv6 ping check
    uint32_t icmp6_timeout_ms;         // timeout for probe (ms)
    bool has_icmp6_timeout;
}dhcpv6_global_t;


typedef struct{
    dhcpv6_global_t global;
    dhcpv6_subnet_t subnets[MAX_SUBNET_V6];
    uint16_t subnet_count;
}dhcpv6_config_t;

/**
 * @brief Load and parse a DHCPv6 configuration file.
 *
 * This function parses an ISC-like dhcpv6.conf syntax and fills the provided
 * configuration structure with global options and per-subnet blocks.
 * The function tolerates inline comments and ignores unknown dhcp6.* options.
 *
 * @param path   Path to the DHCPv6 configuration file.
 * @param config Output structure that will be populated on success.
 * @return 0 on success, -1 on error (e.g., file open failure or parse error).
 */
int load_config_v6(const char *path, dhcpv6_config_t *config);

/**
 * @brief Print the currently loaded DHCPv6 configuration (debug helper).
 *
 * This function dumps global settings and each subnet's settings to stdout,
 * including pools, PD pools, and static hosts.
 *
 * @param config Pointer to the configuration to print.
 */
void dump_config_v6(const dhcpv6_config_t *config);

/**
 * @brief Find the subnet that matches a given IPv6 address.
 *
 * The function checks each configured subnet prefix and returns the first
 * subnet for which the provided IPv6 address belongs to the subnet prefix.
 *
 * @param cfg Pointer to the DHCPv6 configuration.
 * @param ip  IPv6 address to classify.
 * @return Pointer to the matching subnet, or NULL if no subnet matches.
 */
dhcpv6_subnet_t *find_subnet_for_ipv6(const dhcpv6_config_t *cfg, const struct in6_addr *ip);

/**
 * @brief Find a static host entry inside a subnet by its DUID.
 *
 * Iterates the subnet's static host list and compares the stored DUID strings.
 *
 * @param subnet Pointer to the subnet in which to search.
 * @param duid   DUID string of the client (as parsed from config).
 * @return Pointer to the matching host entry, or NULL if not found.
 */
dhcpv6_static_host_t *find_host_by_duid(dhcpv6_subnet_t *subnet,const char *duid);

/**
 * @brief Convert all IPv6 textual fields to their binary representations.
 *
 * Converts:
 * - subnet prefixes (prefix -> prefix_bin)
 * - address pools (pool_start/pool_end -> pool_start_bin/pool_end_bin)
 * - PD pools (pd_pool_start/pd_pool_end -> *_bin)
 * - static host fixed IPv6 addresses (fixed_address6 -> fixed_addr6_bin)
 *
 * This is intended to be called once after parsing to avoid repeated conversions
 * during runtime operations (e.g., subnet lookup, pool management).
 *
 * @param cfg Pointer to the DHCPv6 configuration to update in-place.
 */
void convert_all_to_binary(dhcpv6_config_t *cfg);
#endif