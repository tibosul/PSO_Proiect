#include "config_v4.h"
#include <stdio.h>
#include <arpa/inet.h>

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <config_file>\n", argv[0]);
        return 1;
    }
    
    struct dhcp_config_t config;
    
    printf("Parsing configuration file: %s\n", argv[1]);
    if (parse_config_file(argv[1], &config) != 0)
    {
        fprintf(stderr, "Failed to parse config file\n");
        return 1;
    }
    
    printf("\nConfiguration parsed successfully!\n\n");
    print_config(&config);
    
    // Test subnet lookup
    printf("\n--- Testing subnet lookup ---\n");
    struct in_addr test_ip;
    inet_pton(AF_INET, "192.168.1.150", &test_ip);
    
    struct dhcp_subnet_t *subnet = find_subnet_for_ip(&config, test_ip);
    if (subnet)
    {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &subnet->network, ip_str, INET_ADDRSTRLEN);
        printf("IP 192.168.1.150 belongs to subnet: %s\n", ip_str);
    }
    else
    {
        printf("IP 192.168.1.150 not found in any subnet\n");
    }
    
    // Test host lookup
    printf("\n--- Testing host reservation lookup ---\n");
    uint8_t test_mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    struct dhcp_host_reservation_t *host = find_host_by_mac(subnet, test_mac);
    if (host)
    {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &host->fixed_address, ip_str, INET_ADDRSTRLEN);
        printf("MAC 00:11:22:33:44:55 reserved as: %s (%s)\n", host->name, ip_str);
    }
    else
    {
        printf("MAC 00:11:22:33:44:55 has no reservation\n");
    }
    
    free_config(&config);
    return 0;
}

// Compile: gcc -o test_config_v4 test_config_v4.c config_v4.c
// Run: ./test_config_v4 dhcpv4.conf