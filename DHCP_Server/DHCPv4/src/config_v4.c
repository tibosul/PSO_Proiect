#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "../include/config_v4.h"
#include "../include/string_utils.h"
#include "../include/network_utils.h"

#define MAX_LINE_LEN 1024

static int parse_global_option(char* line, struct dhcp_global_options_t* global)
{
    char* key = strtok(line, " \t;"); 
    if (!key) return -1;

    if(strcmp(key, "authoritative") == 0)
    {
        global->authoritative = true;
        return 0;
    }
    
    // For "option domain-name-servers ..."
    if(strcmp(key, "option") == 0)
    {
        char* opt_name = strtok(NULL, " \t");
        char* opt_value = strtok(NULL, ";");
        
        if(opt_name && opt_value)
        {
            opt_value = trim(opt_value);
            if(strcmp(opt_name, "domain-name-servers") == 0)
            {
                global->dns_server_count = parse_ip_list(opt_value, global->dns_servers, MAX_DNS_SERVERS);
            }
        }
        return 0;
    }
    
    char* value = strtok(NULL, ";");
    if (!value) return -1;
    value = trim(value);
    
    if(strcmp(key, "default-lease-time") == 0)
    {
        global->default_lease_time = atoi(value);
    }
    else if(strcmp(key, "max-lease-time") == 0)
    {
        global->max_lease_time = atoi(value);
    }
    else if(strcmp(key, "ddns-update-style") == 0)
    {
        strncpy(global->ddns_update_style, value, sizeof(global->ddns_update_style) - 1);
    }
    else if(strcmp(key, "ping-check") == 0)
    {
        global->ping_check = (strcmp(value, "true") == 0);
    }
    else if(strcmp(key, "ping-timeout") == 0)
    {
        global->ping_timeout = atoi(value);
    }
    
    return 0;
}

static int parse_subnet_option(char* line, struct dhcp_subnet_t* subnet)
{
    char* token = strtok(line, " \t");
    if(strcmp(token, "range") == 0)
    {
        char* start = strtok(NULL, " \t");
        char* end = strtok(NULL, ";");
        if(start && end)
        {
            parse_ip_address(trim(start), &subnet->range_start);
            parse_ip_address(trim(end), &subnet->range_end);
        }
    }
    else if(strcmp(token, "option") == 0)
    {
        char* opt_name = strtok(NULL, " \t");
        char* opt_value = strtok(NULL, ";");
        if(!opt_name || !opt_value) return -1;

        opt_value = trim(opt_value);

        if(strcmp(opt_name, "routers") == 0)
        {
            parse_ip_address(opt_value, &subnet->router);
        }
        else if(strcmp(opt_name, "broadcast-address") == 0)
        {
            parse_ip_address(opt_value, &subnet->broadcast);
        }
        else if(strcmp(opt_name, "subnet-mask") == 0)
        {
            parse_ip_address(opt_name, &subnet->subnet_mask);
        }
        else if(strcmp(opt_name, "domain-name") == 0)
        {
            // Remove quote -- for example in "domain name example"
            if(opt_value[0] == '"') opt_value++;
            int len = strlen(opt_value);
            if(len > 0 && opt_value[len - 1] == '"') opt_value[len - 1] = '\0';
            strncpy(subnet->domain_name, opt_value, MAX_DOMAIN_LENGTH);
        }
        else if(strcmp(opt_name, "domain-name-servers") == 0)
        {
            subnet->dns_server_count = parse_ip_list(opt_value, subnet->dns_servers, MAX_DNS_SERVERS);
        }
        else if(strcmp(opt_name, "time-offset") == 0)
        {
            subnet->time_offset = atoi(opt_value);
        }
        else if(strcmp(opt_name, "ntp-servers") == 0)
        {
            subnet->ntp_server_count = parse_ip_list(opt_value, subnet->ntp_servers, MAX_NTP_SERVERS);
        }
        else if(strcmp(opt_name, "netbios-name-servers") == 0)
        {
            subnet->netbios_server_count = parse_ip_list(opt_value, subnet->netbios_servers, MAX_NTP_SERVERS);
        }
    }
    else if(strcmp(token, "default-lease-time") == 0)
    {
        char* value = strtok(NULL, ";");
        if(value)
        {
            subnet->default_lease_time = atoi(trim(value));
        }
    }
    else if(strcmp(token, "max-lease-time") == 0)
    {
        char* value = strtok(NULL, ";");
        if(value)
        {
            subnet->max_lease_time = atoi(trim(value));
        }
    }
    return 0;
}

static int parse_host_block(FILE* fp, struct dhcp_subnet_t* subnet)
{
    char line[MAX_LINE_LEN];
    struct dhcp_host_reservation_t* host = &subnet->hosts[subnet->host_count];

    while(fgets(line, sizeof(line), fp))
    {
        char* trimmed = trim(line);

        if(strlen(trimmed) == 0 || trimmed[0] == '#') continue;
        
        if(strchr(trimmed, '}'))
        {
            subnet->host_count++;
            return 0;
        }

        char* token = strtok(trimmed, " \t");
        if(strcmp(token, "hardware-ethernet") == 0)
        {
            char* mac = strtok(NULL, ";");
            if(mac) parse_mac_address(trim(mac), host->mac_address);
        }
        else if(strcmp(token, "fixed-address") == 0)
        {
            char* ip = strtok(NULL, ";");
            if(ip) parse_ip_address(trim(ip), &host->fixed_address);
        }
        else if(strcmp(token, "option") == 0)
        {
            char* opt_name = strtok(NULL, " \t");
            if(opt_name && strcmp(opt_name, "host-name") == 0)
            {
                char* hostname = strtok(NULL, ";");
                if(hostname)
                {
                    hostname = trim(hostname);
                    // Remove quotes
                    if(hostname[0] == '"') hostname++;
                    int len = strlen(hostname);
                    if(len > 0 && hostname[len - 1] == '"') hostname[len - 1] = '\0';
                    strncpy(host->hostname, hostname, MAX_HOSTNAME_LENGTH - 1);
                }
            }
        }
    }
    return -1;
}

static int parse_subnet_block(FILE* fp, struct dhcp_config_t* config, char* first_line)
{
    struct dhcp_subnet_t* subnet = &config->subnets[config->subnet_count];
    memset(subnet, 0, sizeof(struct dhcp_subnet_t));


    // Parse "subnet x.x.x.x netmask y.y.y.y {"
    char* token = strtok(first_line, " \t");
    if(strcmp(token, "subnet") != 0) return -1;

    char* network = strtok(NULL, " \t");
    token = strtok(NULL, " \t"); // "netmask"
    char* netmask = strtok(NULL, " \t{");

    if(network && netmask)
    {
        parse_ip_address(network, &subnet->network);
        parse_ip_address(netmask, &subnet->netmask);

    }

    char line[MAX_LINE_LEN];
    while(fgets(line, sizeof(line), fp))
    {
        char* trimmed = trim(line);

        // Skip empty lines and comments
        if(strlen(trimmed) == 0 || trimmed[0] == '#') continue;

        if(trimmed[0] == '}')
        {
            config->subnet_count++;
            return 0;
        }

        // Host block
        if(strncmp(trimmed, "host", 4) == 0)
        {
            char* host_name = strtok(trimmed + 4, " \t{");
            if(host_name && subnet->host_count < MAX_HOSTS_PER_SUBNET)
            {
                strncpy(subnet->hosts[subnet->host_count].name, trim(host_name), MAX_HOSTNAME_LENGTH - 1);
                parse_host_block(fp, subnet);
            }
        }
        else
        {
            parse_subnet_option(trimmed, subnet);
        }
    }

    return -1;
}

int parse_config_file(const char* filename, struct dhcp_config_t* config)
{
    FILE* fp = fopen(filename, "r");
    if(!fp)
    {
        perror("Failed to open config file!\n");
        return -1;
    }

    memset(config, 0, sizeof(struct dhcp_config_t));
    
    char line[MAX_LINE_LEN];
    while(fgets(line, sizeof(line), fp))
    {
        char* trimmed = trim(line);
        if(strlen(trimmed) == 0 || trimmed[0] == '#') continue;

        if(strncmp(trimmed, "subnet", 6) == 0)
        {
            if(config->subnet_count < MAX_SUBNETS)
            {
                parse_subnet_block(fp, config, trimmed);
            }
        }
        else
        {
            parse_global_option(trimmed, &config->global);
        }
    }

    fclose(fp);
    return 0;
}

struct dhcp_subnet_t* find_subnet_for_ip(struct dhcp_config_t* config, struct in_addr ip)
{
    for(uint32_t i = 0; i < config->subnet_count; i++)
    {
        uint32_t ip_val = ntohl(ip.s_addr);
        uint32_t net_val = ntohl(config->subnets[i].network.s_addr);
        uint32_t mask_val = ntohl(config->subnets[i].netmask.s_addr);

        if ((ip_val & mask_val) == net_val)
        {
            return &config->subnets[i];
        }
    }
    return NULL;
}

struct dhcp_host_reservation_t* find_host_by_mac(struct dhcp_subnet_t* subnet, const uint8_t mac[6])
{
    for(uint32_t i = 0; i < subnet->host_count; i++)
    {
        if(memcmp(subnet->hosts[i].mac_address, mac, 6) == 0)
        {
            return &subnet->hosts[i];
        }
    }
    return NULL;
}

void print_config(const struct dhcp_config_t* config)
{
    printf("--- Global Options ---\n");
    printf("Authoritative: %s\n", config->global.authoritative ? "yes" : "no");
    printf("Default Lease Time: %u seconds\n", config->global.default_lease_time);
    printf("Max Lease Time: %u seconds\n", config->global.max_lease_time);
    printf("DNS Servers: %d\n", config->global.dns_server_count);
    for(uint32_t i = 0; i < config->global.dns_server_count; i++)
    {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &config->global.dns_servers[i], ip_str, INET_ADDRSTRLEN);
        printf("  - %s\n", ip_str);
    }

    printf("\n--- Subnets (%d) ---\n", config->subnet_count);
    for (uint32_t i = 0; i < config->subnet_count; i++)
    {
        const struct dhcp_subnet_t *subnet = &config->subnets[i];
        char ip_str[INET_ADDRSTRLEN];
        
        inet_ntop(AF_INET, &subnet->network, ip_str, INET_ADDRSTRLEN);
        printf("\nSubnet %d: %s\n", i+1, ip_str);
        
        inet_ntop(AF_INET, &subnet->range_start, ip_str, INET_ADDRSTRLEN);
        printf("  Range Start: %s\n", ip_str);
        
        inet_ntop(AF_INET, &subnet->range_end, ip_str, INET_ADDRSTRLEN);
        printf("  Range End: %s\n", ip_str);
        
        printf("  Domain: %s\n", subnet->domain_name);
        printf("  Host Reservations: %d\n", subnet->host_count);
        
        for (uint32_t j = 0; j < subnet->host_count; j++)
        {
            printf("    - %s: %02x:%02x:%02x:%02x:%02x:%02x\n",
                   subnet->hosts[j].name,
                   subnet->hosts[j].mac_address[0],
                   subnet->hosts[j].mac_address[1],
                   subnet->hosts[j].mac_address[2],
                   subnet->hosts[j].mac_address[3],
                   subnet->hosts[j].mac_address[4],
                   subnet->hosts[j].mac_address[5]);
        }
    }
}

void free_config(struct dhcp_config_t* config)
{
    memset(config, 0, sizeof(struct dhcp_config_t));
}