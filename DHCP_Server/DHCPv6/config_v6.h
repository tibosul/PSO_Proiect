#ifndef CONFIG_V6_H
#define CONFIG_V6_H

#include <stdint.h>
#include <stdbool.h>



#define HOSTNAME_MAX 256
#define Ip6_STR_MAX 80
#define DUID_MAX 130
#define MAX_SUBNET_V6 8
#define MAX_HOSTS_PER_SUBNET 200

typedef struct 
{
    char hostname[HOSTNAME_MAX];
    char duid[DUID_MAX];
    char fixed_address6[Ip6_STR_MAX];
}dhcpv6_static_host_t;

typedef struct 
{
   char prefix[Ip6_STR_MAX];
   uint8_t prefix_len;

   char pool_start[Ip6_STR_MAX];
   char pool_end[Ip6_STR_MAX];

   char dns_servers[256];
   char domain_search[256];

   uint32_t default_lease_time;
   uint32_t max_lease_time;    

   dhcpv6_static_host_t hosts[MAX_HOSTS_PER_SUBNET];
   uint16_t host_count;


   bool pd_enabled;

   char pd_pool_start[Ip6_STR_MAX];
   char pd_pool_end[Ip6_STR_MAX];
   uint8_t pd_prefix_len;
}dhcpv6_subnet_t;


typedef struct{
    uint32_t default_lease_time;
    uint32_t max_lease_time;
    char global_dns_servers[256];
    char global_domain_search[256];
}dhcpv6_global_t;


typedef struct{
    dhcpv6_global_t global;
    dhcpv6_subnet_t subnets[MAX_SUBNET_V6];
    uint16_t subnet_count;
}dhcpv6_config_t;


int load_config_v6(const char *path, dhcpv6_config_t *config);
void dump_config_v6(const dhcpv6_config_t *config);



#endif