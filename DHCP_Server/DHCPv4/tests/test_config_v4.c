#include "../include/config_v4.h"
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

    printf("=============================================================\n");
    printf("DHCPv4 Configuration Parser Test\n");
    printf("=============================================================\n");
    printf("Parsing configuration file: %s\n", argv[1]);

    if (parse_config_file(argv[1], &config) != 0)
    {
        fprintf(stderr, "❌ Failed to parse config file\n");
        return 1;
    }

    printf("✓ Configuration parsed successfully!\n\n");
    print_config(&config);

    // Summary statistics
    printf("\n=============================================================\n");
    printf("Configuration Summary\n");
    printf("=============================================================\n");
    printf("Total Subnets: %u\n", config.subnet_count);

    uint32_t total_hosts = 0;
    for (uint32_t i = 0; i < config.subnet_count; i++)
    {
        total_hosts += config.subnets[i].host_count;
    }
    printf("Total Host Reservations: %u\n", total_hosts);
    printf("Global DNS Servers: %u\n", config.global.dns_server_count);
    printf("Authoritative: %s\n", config.global.authoritative ? "Yes" : "No");
    printf("Default Lease Time: %u seconds (%u hours)\n",
           config.global.default_lease_time,
           config.global.default_lease_time / 3600);
    printf("Max Lease Time: %u seconds (%u hours)\n",
           config.global.max_lease_time,
           config.global.max_lease_time / 3600);
    printf("Ping Check: %s (timeout: %u sec)\n",
           config.global.ping_check ? "Enabled" : "Disabled",
           config.global.ping_timeout);

    // Test subnet lookups for different networks
    printf("\n=============================================================\n");
    printf("Testing Subnet Lookups\n");
    printf("=============================================================\n");

    const char* test_ips[] = {
        "192.168.1.150",    // Corporate LAN
        "10.0.0.75",        // Guest WiFi
        "10.10.0.120",      // IoT Network
        "172.16.100.100",   // VoIP Network
        "192.168.50.150",   // Dev Network
        "203.0.113.7"       // DMZ Network
    };

    for (int i = 0; i < 6; i++)
    {
        struct in_addr test_ip;
        inet_pton(AF_INET, test_ips[i], &test_ip);

        struct dhcp_subnet_t *subnet = find_subnet_for_ip(&config, test_ip);
        if (subnet)
        {
            char net_str[INET_ADDRSTRLEN];
            char mask_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &subnet->network, net_str, INET_ADDRSTRLEN);
            inet_ntop(AF_INET, &subnet->netmask, mask_str, INET_ADDRSTRLEN);
            printf("✓ IP %s → Subnet %s/%s (domain: %s)\n",
                   test_ips[i], net_str, mask_str, subnet->domain_name);
        }
        else
        {
            printf("✗ IP %s → Not found in any subnet\n", test_ips[i]);
        }
    }

    // Test host reservations
    printf("\n=============================================================\n");
    printf("Testing Host Reservations\n");
    printf("=============================================================\n");

    struct {
        const char* description;
        uint8_t mac[6];
        const char* expected_subnet;
    } test_hosts[] = {
        {"DC01 (Corporate)", {0x00, 0x11, 0x22, 0x33, 0x44, 0x55}, "192.168.1.0"},
        {"Camera Entrance (IoT)", {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x01}, "10.10.0.0"},
        {"Phone Conf Room A (VoIP)", {0x11, 0x22, 0x33, 0x44, 0x55, 0x01}, "172.16.100.0"},
        {"Jenkins CI (Dev)", {0xde, 0xad, 0x10, 0x00, 0x00, 0x10}, "192.168.50.0"}
    };

    for (int i = 0; i < 4; i++)
    {
        bool found = false;
        for (uint32_t j = 0; j < config.subnet_count; j++)
        {
            struct dhcp_host_reservation_t *host =
                find_host_by_mac(&config.subnets[j], test_hosts[i].mac);

            if (host)
            {
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &host->fixed_address, ip_str, INET_ADDRSTRLEN);
                printf("✓ %s: %02x:%02x:%02x:%02x:%02x:%02x → %s (%s)\n",
                       test_hosts[i].description,
                       test_hosts[i].mac[0], test_hosts[i].mac[1], test_hosts[i].mac[2],
                       test_hosts[i].mac[3], test_hosts[i].mac[4], test_hosts[i].mac[5],
                       ip_str, host->name);
                found = true;
                break;
            }
        }

        if (!found)
        {
            printf("✗ %s: %02x:%02x:%02x:%02x:%02x:%02x → Not found\n",
                   test_hosts[i].description,
                   test_hosts[i].mac[0], test_hosts[i].mac[1], test_hosts[i].mac[2],
                   test_hosts[i].mac[3], test_hosts[i].mac[4], test_hosts[i].mac[5]);
        }
    }

    printf("\n=============================================================\n");
    printf("All tests completed!\n");
    printf("=============================================================\n");

    free_config(&config);
    return 0;
}

// Compile: gcc -o test_config_v4 test_config_v4.c config_v4.c
// Run: ./test_config_v4 dhcpv4.conf