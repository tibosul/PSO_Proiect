#include<stdio.h>
#include<string.h>
#include<arpa/inet.h>
#include "config_v6.h"
#include "utilsv6.h"

int main(int argc, char *argv[])
{
    const char *config_path = "dhcpv6.conf";

    if(argc>1)
    {
        config_path=argv[1];
    }

    dhcpv6_config_t cfg;
    memset(&cfg,0,sizeof(cfg));

    printf("Loadind DHCPv6 configuration from %s\n",config_path);
    if(load_config_v6(config_path,&cfg)!=0)
    {
        fprintf(stderr,"Failed to load configuration\n");
        return 1;
    }

    convert_all_to_binary(&cfg);
  
    printf("Configuration loaded successfully\n\n");
    dump_config_v6(&cfg);

    printf("Test 1. Find subnet for an ipv6 address\n");
    struct in6_addr test_ip;
    const char *test_ip_str="2001:db8:1:0::1050";
    if(ipv6_str_to_bin(test_ip_str,&test_ip)==0)
    {
        dhcpv6_subnet_t *sub=find_subnet_for_ipv6(&cfg,&test_ip);
        if(sub)
        {
            printf("%s belongs to subnet %s/%u\n",test_ip_str,sub->prefix,sub->prefix_len);
        }
        else{
            printf("%s is not found in any subnet\n",test_ip_str);
        }
    }
    else{
        printf("INvalid ipv6 string %s\n",test_ip_str);
    }

    printf("Test 2. Find host by DUID in a subnet\n");
    const char *duid_to_find="00:01:00:01:23:45:67:89:ab:cd:ef:01:23:45";
    dhcpv6_static_host_t *host = find_host_by_duid(&cfg.subnets[0], duid_to_find);
    if (host) {
        printf("Found host: %s\n", host->hostname);
        printf("DUID: %s\n", host->duid);
        printf("IP:   %s\n", host->fixed_address6);
    } else {
        printf("Host with DUID %s not found.\n", duid_to_find);
    }

    printf("Test 3: Binary comparison of prefix range\n");

    char ip_in_range_str[] = "2001:db8:1:0::1abc";
    char ip_out_of_range_str[] = "2001:db8:5::1234";
    struct in6_addr ip_in, ip_out;

    ipv6_str_to_bin(ip_in_range_str, &ip_in);
    ipv6_str_to_bin(ip_out_of_range_str, &ip_out);

    if (find_subnet_for_ipv6(&cfg, &ip_in))
        printf("%s is inside one of the defined subnets\n", ip_in_range_str);
    else
        printf("%s not found in any subnet\n", ip_in_range_str);

    if (find_subnet_for_ipv6(&cfg, &ip_out))
        printf("%s incorrectly matched a subnet\n", ip_out_of_range_str);
    else
        printf("%s correctly not found in any subnet\n", ip_out_of_range_str);

    printf("Test 4: Print prefix delegation details (if any)\n");

    for (int i = 0; i < cfg.subnet_count; i++) {
        dhcpv6_subnet_t *s = &cfg.subnets[i];
        if (s->pd_enabled) {
            printf("Subnet %s/%u has PD pool:\n", s->prefix, s->prefix_len);
            printf("Start: %s\n", s->pd_pool_start);
            printf("End:   %s\n", s->pd_pool_end);
            printf("Prefix len per client: /%u\n", s->pd_prefix_len);
        }
    }

    printf("\nAll tests finished.\n");
    return 0;
}