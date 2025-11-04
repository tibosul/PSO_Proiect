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

   char sntp_servers[256];       // "" = neconfigurat
   bool has_sntp_servers;

   uint32_t info_refresh_time;   // 0 = neconfigurat
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
}dhcpv6_global_t;


typedef struct{
    dhcpv6_global_t global;
    dhcpv6_subnet_t subnets[MAX_SUBNET_V6];
    uint16_t subnet_count;
}dhcpv6_config_t;


int load_config_v6(const char *path, dhcpv6_config_t *config);
void dump_config_v6(const dhcpv6_config_t *config);
dhcpv6_subnet_t *find_subnet_for_ipv6(const dhcpv6_config_t *cfg, const struct in6_addr *ip);
dhcpv6_static_host_t *find_host_by_duid(dhcpv6_subnet_t *subnet,const char *duid);

void convert_all_to_binary(dhcpv6_config_t *cfg);
#endif