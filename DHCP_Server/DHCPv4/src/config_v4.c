#define _GNU_SOURCE

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "../include/config_v4.h"
#include "../include/string_utils.h"
#include "../include/file_utils.h"
#include "../include/network_utils.h"

#define MAX_LINE_LEN 1024

/**
 * @brief Parse a DHCPv4 global option line.
 * @param line Line containing the global option.
 * @param global Pointer to the global options structure to populate.
 * @return 0 on success,
 *         -1 if line or global is NULL,
 *         -2 if parsing fails in any way
 */
static int parse_global_option(char *line, struct dhcp_global_options_t *global);

/**
 * @brief Parse a DHCPv4 subnet option line.
 * @param line Line containing the subnet option.
 * @param subnet Pointer to the subnet structure to populate.
 * @return 0 on success,
 *         -1 if line or subnet is NULL,
 *         -2 if parsing fails in any way
 */
static int parse_subnet_option(char *line, struct dhcp_subnet_t *subnet);

/**
 * @brief Parse a host reservation block within a subnet.
 * @param fp File pointer to read from.
 * @param subnet Pointer to the subnet structure to populate.
 * @return 0 on success (host_count incremented),
 *         -1 if fp or subnet is NULL,
 *         -2 if parsing fails for host options,
 *         -3 if maximum host count is reached
 */
static int parse_host_block(FILE *fp, struct dhcp_subnet_t *subnet);

/**
 * @brief Parse a subnet block from the configuration file.
 * @param fp File pointer to read from.
 * @param config Pointer to the DHCP configuration structure.
 * @param first_line The first line of the subnet block (already read).
 * @return 0 on success (subnet_count incremented),
 *         -1 if fp, config, or first_line is NULL,
 *         -2 if parsing fails for subnet parameters or options,
 *         -3 if maximum subnet count is reached
 */
static int parse_subnet_block(FILE *fp, struct dhcp_config_t *config, char *first_line);

/**
 * @brief Convert DDNS update style enum to string representation.
 * @param style DDNS update style enumeration value.
 * @return String representation of the style, or "unknown" for invalid values.
 */
const char *ddns_update_style_to_string(ddns_update_style_t style)
{
    switch (style)
    {
    case DDNS_NONE:
        return "none";
    case DDNS_INTERIM:
        return "interim";
    case DDNS_STANDARD:
        return "standard";
    case DDNS_AD_HOC:
        return "ad-hoc";
    default:
        return "unknown";
    }
}

/**
 * @brief Convert string to DDNS update style enum.
 * @param str DDNS style string (e.g., "none", "interim", "standard", "ad-hoc").
 * @return DDNS update style enumeration value, or DDNS_UNKNOWN if str is NULL or unrecognized.
 */
ddns_update_style_t ddns_update_style_from_string(const char *str)
{
    if (!str)
        return DDNS_UNKNOWN;

    if (strcmp(str, "none") == 0)
        return DDNS_NONE;
    else if (strcmp(str, "interim") == 0)
        return DDNS_INTERIM;
    else if (strcmp(str, "standard") == 0)
        return DDNS_STANDARD;
    else if (strcmp(str, "ad-hoc") == 0)
        return DDNS_AD_HOC;
    else
        return DDNS_UNKNOWN;
}

static int parse_global_option(char *line, struct dhcp_global_options_t *global)
{
    if (!line || !global)
        return -1;

    char *key = strtok(line, " \t;");
    if (!key)
        return -2;

    if (strcmp(key, "authoritative") == 0)
    {
        global->authoritative = true;
        return 0;
    }

    // For "option domain-name-servers ..."
    if (strcmp(key, "option") == 0)
    {
        char *opt_name = strtok(NULL, " \t");
        char *opt_value = strtok(NULL, ";");

        if (!opt_name || !opt_value)
            return -2;
        else
        {
            opt_value = trim(opt_value);
            if (!opt_value)
                return -2;

            if (strcmp(opt_name, "domain-name-servers") == 0)
            {
                int count = parse_ip_list(opt_value, global->dns_servers, MAX_DNS_SERVERS);
                if (count < 0)
                    return -2;
                global->dns_server_count = count;
            }
            else if (strcmp(opt_name, "ntp-servers") == 0)
            {
                // DHCP option 42 - NTP servers
                int count = parse_ip_list(opt_value, global->ntp_servers, MAX_NTP_SERVERS);
                if (count < 0)
                    return -2;
                global->ntp_server_count = count;
            }
            else if (strcmp(opt_name, "netbios-name-servers") == 0)
            {
                // DHCP option 44 - NetBIOS name servers
                int count = parse_ip_list(opt_value, global->netbios_servers, MAX_NETBIOS_SERVERS);
                if (count < 0)
                    return -2;
                global->netbios_server_count = count;
            }
            else if (strcmp(opt_name, "time-offset") == 0)
            {
                // DHCP option 2 - Time offset from UTC (in seconds)
                if (parse_uint32(opt_value, (uint32_t *)&global->time_offset) != 0)
                    return -2;
            }
            else if (strcmp(opt_name, "tftp-server-name") == 0)
            {
                // DHCP option 66 - TFTP server name or IP
                opt_value = remove_quotes(opt_value);
                if (!opt_value)
                    return -2;
                strncpy(global->tftp_server_name, opt_value, sizeof(global->tftp_server_name) - 1);
            }
            else if (strcmp(opt_name, "bootfile-name") == 0)
            {
                // DHCP option 67 - Boot file name
                opt_value = remove_quotes(opt_value);
                if (!opt_value)
                    return -2;
                strncpy(global->bootfile_name, opt_value, sizeof(global->bootfile_name) - 1);
            }
            else if (strcmp(opt_name, "dhcp-renewal-time") == 0)
            {
                // DHCP option 58 (T1) - Renewal time
                if (parse_uint32(opt_value, &global->renewal_time) != 0)
                    return -2;
            }
            else if (strcmp(opt_name, "dhcp-rebinding-time") == 0)
            {
                // DHCP option 59 (T2) - Rebinding time
                if (parse_uint32(opt_value, &global->rebinding_time) != 0)
                    return -2;
            }
        }
        return 0;
    }

    char *value = strtok(NULL, ";");
    if (!value)
        return -2;
    value = trim(value);
    if (!value)
        return -2;

    if (strcmp(key, "default-lease-time") == 0)
    {
        if (parse_uint32(value, &global->default_lease_time) != 0)
            return -2;
    }
    else if (strcmp(key, "max-lease-time") == 0)
    {
       if (parse_uint32(value, &global->max_lease_time) != 0)
            return -2;
    }
    else if (strcmp(key, "ddns-update-style") == 0)
    {
        global->ddns_update_style = ddns_update_style_from_string(value);
    }
    else if (strcmp(key, "ping-check") == 0)
    {
        global->ping_check = (strcmp(value, "true") == 0);
    }
    else if (strcmp(key, "ping-timeout") == 0)
    {
        if (parse_uint32(value, &global->ping_timeout) != 0)
            return -2;
    }
    else if (strcmp(key, "next-server") == 0)
    {
        // Boot server IP address for PXE
        if (parse_ip_address(value, &global->next_server) != 0)
            return -2;
    }
    else if (strcmp(key, "filename") == 0)
    {
        // Boot file name for PXE
        value = remove_quotes(value);
        if (!value)
            return -2;
        strncpy(global->filename, value, sizeof(global->filename) - 1);
    }
    else if (strcmp(key, "allow") == 0)
    {
        // Parse "allow unknown-clients", "allow bootp", etc.
        if (strcmp(value, "unknown-clients") == 0)
        {
            global->allow_unknown_clients = true;
        }
        else if (strcmp(value, "bootp") == 0)
        {
            global->allow_bootp = true;
        }
    }
    else if (strcmp(key, "deny") == 0)
    {
        // Parse "deny unknown-clients", "deny bootp", etc.
        if (strcmp(value, "unknown-clients") == 0)
        {
            global->allow_unknown_clients = false;
        }
        else if (strcmp(value, "bootp") == 0)
        {
            global->allow_bootp = false;
        }
    }
    else if (strcmp(key, "update-conflict-detection") == 0)
    {
        global->update_conflict_detection = (strcmp(value, "true") == 0);
    }

    return 0;
}

static int parse_subnet_option(char *line, struct dhcp_subnet_t *subnet)
{
    if (!line || !subnet)
        return -1;

    char *token = strtok(line, " \t");
    if (!token)
        return -2;

    if (strcmp(token, "range") == 0)
    {
        char *start = strtok(NULL, " \t");
        char *end = strtok(NULL, ";");
        if (start && end)
        {
            start = trim(start);
            end = trim(end);
            if (!start || !end)
                return -2;
            if (parse_ip_address(start, &subnet->range_start) != 0)
                return -2;
            if (parse_ip_address(end, &subnet->range_end) != 0)
                return -2;
        }
    }
    else if (strcmp(token, "option") == 0)
    {
        char *opt_name = strtok(NULL, " \t");
        char *opt_value = strtok(NULL, ";");
        if (!opt_name || !opt_value)
            return -2;

        opt_value = trim(opt_value);
        if (!opt_value)
            return -2;

        if (strcmp(opt_name, "routers") == 0)
        {
            if (parse_ip_address(opt_value, &subnet->router) != 0)
                return -2;
        }
        else if (strcmp(opt_name, "broadcast-address") == 0)
        {
            if (parse_ip_address(opt_value, &subnet->broadcast) != 0)
                return -2;
        }
        else if (strcmp(opt_name, "subnet-mask") == 0)
        {
            if (parse_ip_address(opt_value, &subnet->subnet_mask) != 0)
                return -2;
        }
        else if (strcmp(opt_name, "domain-name") == 0)
        {
            // Remove quote -- for example in "domain name example"
            opt_value = remove_quotes(opt_value);
            if (!opt_value)
                return -2;
            strncpy(subnet->domain_name, opt_value, MAX_DOMAIN_LENGTH);
        }
        else if (strcmp(opt_name, "domain-name-servers") == 0)
        {
            int count = parse_ip_list(opt_value, subnet->dns_servers, MAX_DNS_SERVERS);
            if (count < 0)
                return -2;
            subnet->dns_server_count = count;
        }
        else if (strcmp(opt_name, "time-offset") == 0)
        {
            if (parse_uint32(opt_value, (uint32_t *)&subnet->time_offset) != 0)
                return -2;
        }
        else if (strcmp(opt_name, "ntp-servers") == 0)
        {
            int count = parse_ip_list(opt_value, subnet->ntp_servers, MAX_NTP_SERVERS);
            if (count < 0)
                return -2;
            subnet->ntp_server_count = count;
        }
        else if (strcmp(opt_name, "netbios-name-servers") == 0)
        {
            int count = parse_ip_list(opt_value, subnet->netbios_servers, MAX_NTP_SERVERS);
            if (count < 0)
                return -2;
            subnet->netbios_server_count = count;
        }
        else if (strcmp(opt_name, "tftp-server-name") == 0)
        {
            // DHCP option 66 - TFTP server name or IP (subnet override)
            opt_value = remove_quotes(opt_value);
            if (!opt_value)
                return -2;
            strncpy(subnet->tftp_server_name, opt_value, sizeof(subnet->tftp_server_name) - 1);
        }
        else if (strcmp(opt_name, "bootfile-name") == 0)
        {
            // DHCP option 67 - Boot file name (subnet override)
            opt_value = remove_quotes(opt_value);
            if (!opt_value)
                return -2;
            strncpy(subnet->bootfile_name, opt_value, sizeof(subnet->bootfile_name) - 1);
        }
        else if (strcmp(opt_name, "dhcp-renewal-time") == 0)
        {
            // DHCP option 58 (T1) - Renewal time (subnet override)
            if (parse_uint32(opt_value, &subnet->renewal_time) != 0)
                return -2;
        }
        else if (strcmp(opt_name, "dhcp-rebinding-time") == 0)
        {
            // DHCP option 59 (T2) - Rebinding time (subnet override)
            if (parse_uint32(opt_value, &subnet->rebinding_time) != 0)
                return -2;
        }
    }
    else if (strcmp(token, "default-lease-time") == 0)
    {
        char *value = strtok(NULL, ";");
        if (value)
        {
            value = trim(value);
            if (!value)
                return -2;
            if (parse_uint32(value, &subnet->default_lease_time) != 0)
                return -2;
        }
    }
    else if (strcmp(token, "max-lease-time") == 0)
    {
        char *value = strtok(NULL, ";");
        if (value)
        {
            value = trim(value);
            if (!value)
                return -2;
            if (parse_uint32(value, &subnet->max_lease_time) != 0)
                return -2;
        }
    }
    else if (strcmp(token, "next-server") == 0)
    {
        // Boot server IP address for PXE (subnet override)
        char *value = strtok(NULL, ";");
        if (value)
        {
            value = trim(value);
            if (!value)
                return -2;
            if (parse_ip_address(value, &subnet->next_server) != 0)
                return -2;
        }
    }
    else if (strcmp(token, "filename") == 0)
    {
        // Boot file name for PXE (subnet override)
        char *value = strtok(NULL, ";");
        if (value)
        {
            value = trim(value);
            if (!value)
                return -2;
            value = remove_quotes(value);
            if (!value)
                return -2;
            strncpy(subnet->filename, value, sizeof(subnet->filename) - 1);
        }
    }
    return 0;
}

static int parse_host_block(FILE *fp, struct dhcp_subnet_t *subnet)
{
    if (!fp || !subnet)
        return -1;

    if (subnet->host_count >= MAX_HOSTS_PER_SUBNET)
        return -3;

    char line[MAX_LINE_LEN];
    struct dhcp_host_reservation_t *host = &subnet->hosts[subnet->host_count];

    while (fgets(line, sizeof(line), fp))
    {
        char *trimmed = trim(line);
        if (!trimmed)
        {
            fprintf(stderr, "Warning: trim() failed in host block\n");
            return -2;
        }

        // Ignore empty lines and comments
        if (strlen(trimmed) == 0 || trimmed[0] == '#')
            continue;

        if (strchr(trimmed, '}'))
        {
            subnet->host_count++;
            break;
        }

        char *token = strtok(trimmed, " \t");
        if (!token)
            continue;

        if (strcmp(token, "hardware-ethernet") == 0)
        {
            char *mac = strtok(NULL, ";");
            if (mac)
            {
                mac = trim(mac);
                if (!mac)
                {
                    fprintf(stderr, "Warning: Invalid MAC address format in host block\n");
                    return -2;
                }
                if (parse_mac_address(mac, host->mac_address) != 0)
                {
                    fprintf(stderr, "Warning: Failed to parse MAC address in host block\n");
                    return -2;
                }
            }
        }
        else if (strcmp(token, "fixed-address") == 0)
        {
            char *ip = strtok(NULL, ";");
            if (ip)
            {
                ip = trim(ip);
                if (!ip)
                {
                    fprintf(stderr, "Warning: Invalid IP address format in host block\n");
                    return -2;
                }
                if (parse_ip_address(ip, &host->fixed_address) != 0)
                {
                    fprintf(stderr, "Warning: Failed to parse fixed IP address in host block\n");
                    return -2;
                }
            }
        }
        else if (strcmp(token, "option") == 0)
        {
            char *opt_name = strtok(NULL, " \t");
            if (opt_name && strcmp(opt_name, "host-name") == 0)
            {
                char *hostname = strtok(NULL, ";");
                if (hostname)
                {
                    hostname = trim(hostname);
                    if (!hostname)
                    {
                        fprintf(stderr, "Warning: Invalid hostname format in host block\n");
                        return -2;
                    }
                    hostname = remove_quotes(hostname);
                    if (!hostname)
                    {
                        fprintf(stderr, "Warning: Failed to remove quotes from hostname\n");
                        return -2;
                    }
                    strncpy(host->hostname, hostname, MAX_HOSTNAME_LENGTH - 1);
                }
            }
        }
    }

    return 0;
}

static int parse_subnet_block(FILE *fp, struct dhcp_config_t *config, char *first_line)
{
    if (!fp || !config || !first_line)
        return -1;

    if (config->subnet_count >= MAX_SUBNETS)
        return -3;

    struct dhcp_subnet_t *subnet = &config->subnets[config->subnet_count];
    memset(subnet, 0, sizeof(struct dhcp_subnet_t));

    // Parse "subnet x.x.x.x netmask y.y.y.y {"
    char *token = strtok(first_line, " \t");
    if (!token || strcmp(token, "subnet") != 0)
    {
        fprintf(stderr, "Warning: Invalid subnet declaration\n");
        return -2;
    }

    char *network = strtok(NULL, " \t");
    token = strtok(NULL, " \t"); // "netmask"
    char *netmask = strtok(NULL, " \t{");

    if (network && netmask)
    {
        if (parse_ip_address(network, &subnet->network) != 0)
        {
            fprintf(stderr, "Warning: Failed to parse subnet network address\n");
            return -2; // Critical error - cannot proceed without valid network
        }
        if (parse_ip_address(netmask, &subnet->netmask) != 0)
        {
            fprintf(stderr, "Warning: Failed to parse subnet netmask\n");
            return -2; // Critical error - cannot proceed without valid netmask
        }
    }
    else
    {
        fprintf(stderr, "Warning: Missing network or netmask in subnet declaration\n");
        return -2; // Critical error - cannot proceed without valid network/netmask
    }

    char line[MAX_LINE_LEN];

    while (fgets(line, sizeof(line), fp))
    {
        char *trimmed = trim(line);
        if (!trimmed)
        {
            fprintf(stderr, "Warning: trim() failed in subnet block\n");
            return -2;
        }

        // Skip empty lines and comments
        if (strlen(trimmed) == 0 || trimmed[0] == '#')
            continue;

        if (trimmed[0] == '}')
        {
            config->subnet_count++;
            break;
        }

        // Host block
        if (strncmp(trimmed, "host", 4) == 0)
        {
            char *host_name = strtok(trimmed + 4, " \t{");
            if (host_name && subnet->host_count < MAX_HOSTS_PER_SUBNET)
            {
                host_name = trim(host_name);
                if (!host_name)
                {
                    fprintf(stderr, "Warning: Invalid host name in subnet\n");
                    advance_to_next_closed_brace(fp);
                    continue;
                }
                strncpy(subnet->hosts[subnet->host_count].name, host_name, MAX_HOSTNAME_LENGTH - 1);

                int result = parse_host_block(fp, subnet);

                // Note: host_count is only incremented in parse_host_block on success
                // So if there was an error, we don't increment host_count and overwrite the same slot
                if (result != 0)
                {
                    fprintf(stderr, "Warning: Host '%s' has errors, will be overwriten\n", host_name);
                }
            }
        }
        else
        {
            int result = parse_subnet_option(trimmed, subnet);
            if (result != 0)
            {
                fprintf(stderr, "Warning: Failed to parse subnet option\n");
                return -2;
            }
        }
    }

    // Apply global fallbacks for lease times

    if (subnet->default_lease_time == 0)
        subnet->default_lease_time = config->global.default_lease_time;

    if (subnet->max_lease_time == 0)
        subnet->max_lease_time = config->global.max_lease_time;

    // Apply global fallbacks for lease times
    if (subnet->renewal_time == 0)
        subnet->renewal_time = config->global.renewal_time;

    if (subnet->rebinding_time == 0)
        subnet->rebinding_time = config->global.rebinding_time;

    // Apply global fallbacks for PXE boot
    if (subnet->next_server.s_addr == 0)
        subnet->next_server = config->global.next_server;

    if (strlen(subnet->filename) == 0)
        strncpy(subnet->filename, config->global.filename, sizeof(subnet->filename) - 1);

    if (strlen(subnet->tftp_server_name) == 0)
        strncpy(subnet->tftp_server_name, config->global.tftp_server_name, sizeof(subnet->tftp_server_name) - 1);

    if (strlen(subnet->bootfile_name) == 0)
        strncpy(subnet->bootfile_name, config->global.bootfile_name, sizeof(subnet->bootfile_name) - 1);

    // Apply global fallbacks for time and server options
    if (subnet->ntp_server_count == 0 && config->global.ntp_server_count > 0)
    {
        memcpy(subnet->ntp_servers, config->global.ntp_servers,
               config->global.ntp_server_count * sizeof(struct in_addr));
        subnet->ntp_server_count = config->global.ntp_server_count;
    }

    if (subnet->netbios_server_count == 0 && config->global.netbios_server_count > 0)
    {
        memcpy(subnet->netbios_servers, config->global.netbios_servers,
               config->global.netbios_server_count * sizeof(struct in_addr));
        subnet->netbios_server_count = config->global.netbios_server_count;
    }

    if (subnet->time_offset == 0 && config->global.time_offset != 0)
        subnet->time_offset = config->global.time_offset;

    // Apply global fallbacks for DNS (if not already set)
    if (subnet->dns_server_count == 0 && config->global.dns_server_count > 0)
    {
        memcpy(subnet->dns_servers, config->global.dns_servers,
               config->global.dns_server_count * sizeof(struct in_addr));
        subnet->dns_server_count = config->global.dns_server_count;
    }

    return 0;
}

int parse_config_file(const char *filename, struct dhcp_config_t *config)
{
    if (!filename || !config)
        return -1;

    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        perror("Failed to open config file!\n");
        return -1;
    }

    memset(config, 0, sizeof(struct dhcp_config_t));
    config->global.allow_unknown_clients = true; // Default
    config->global.allow_bootp = true;           // Default

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp))
    {
        char *trimmed = trim(line);
        if (!trimmed)
        {
            fprintf(stderr, "Warning: trim() failed in config file, skipping line\n");
            continue;
        }

        if (strlen(trimmed) == 0 || trimmed[0] == '#')
            continue;

        if (strncmp(trimmed, "subnet", 6) == 0)
        {
            int result = parse_subnet_block(fp, config, trimmed);

            // Note: subnet_count is only incremented in parse_subnet_block on success
            // So if there was an error, we don't increment subnet_count and overwrite the same slot
            if (result != 0)
            {
                fprintf(stderr, "Warning: Failed to parse subnet block, continuing with other entries\n");
                advance_to_next_closed_brace(fp); // Skip to the end of the faulty subnet block
                // Continue parsing other subnets
            }
        }
        else
        {
            int result = parse_global_option(trimmed, &config->global);
            if (result != 0)
            {
                fprintf(stderr, "Warning: Failed to parse global option, continuing with defaults\n");
                // Continue parsing other options
            }
        }
    }

    fclose(fp);

    // Print summary
    fprintf(stderr, "\n=== Configuration Parse Summary ===\n");
    fprintf(stderr, "Subnets loaded: %u\n", config->subnet_count);
    fprintf(stderr, "===================================\n\n");


    

    return 0;
}

struct dhcp_subnet_t *find_subnet_for_ip(const struct dhcp_config_t *config, const struct in_addr ip)
{
    if (!config)
        return NULL;

    for (uint32_t i = 0; i < config->subnet_count; i++)
    {
        uint32_t ip_val = ntohl(ip.s_addr);
        uint32_t net_val = ntohl(config->subnets[i].network.s_addr);
        uint32_t mask_val = ntohl(config->subnets[i].netmask.s_addr);

        if ((ip_val & mask_val) == net_val)
        {
            return (struct dhcp_subnet_t *)&config->subnets[i];
        }
    }
    return NULL;
}

struct dhcp_host_reservation_t *find_host_by_mac(const struct dhcp_subnet_t *subnet, const uint8_t mac[6])
{
    if (!subnet || !mac)
        return NULL;

    for (uint32_t i = 0; i < subnet->host_count; i++)
    {
        if (memcmp(subnet->hosts[i].mac_address, mac, 6) == 0)
        {
            return (struct dhcp_host_reservation_t *)&subnet->hosts[i];
        }
    }
    return NULL;
}

void print_config(const struct dhcp_config_t *config)
{
    char ip_str[INET_ADDRSTRLEN];

    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                    GLOBAL DHCP OPTIONS                         ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");

    // Server Behavior
    printf("  Server Behavior:\n");
    printf("    Authoritative:          %s\n", config->global.authoritative ? "yes" : "no");
    printf("    Allow Unknown Clients:  %s\n", config->global.allow_unknown_clients ? "yes" : "no");
    printf("    Allow BOOTP:            %s\n", config->global.allow_bootp ? "yes" : "no");
    printf("    Update Conflict Detection: %s\n", config->global.update_conflict_detection ? "yes" : "no");
    printf("    Ping Check:             %s\n", config->global.ping_check ? "yes" : "no");
    if (config->global.ping_check)
        printf("    Ping Timeout:           %u seconds\n", config->global.ping_timeout);

    const char *ddns_style_str = ddns_update_style_to_string(config->global.ddns_update_style);

    printf("    DDNS Update Style:      %s\n", ddns_style_str);
    printf("\n");

    // Lease Times
    printf("  Lease Times:\n");
    printf("    Default Lease Time:     %u seconds (%f hours)\n",
           config->global.default_lease_time, config->global.default_lease_time / 3600.f);
    printf("    Max Lease Time:         %u seconds (%f hours)\n",
           config->global.max_lease_time, config->global.max_lease_time / 3600.f);
    if (config->global.renewal_time > 0)
        printf("    Renewal Time (T1):      %u seconds\n", config->global.renewal_time);
    if (config->global.rebinding_time > 0)
        printf("    Rebinding Time (T2):    %u seconds\n", config->global.rebinding_time);
    printf("\n");

    // DNS Servers
    printf("  DNS Servers (%u):\n", config->global.dns_server_count);
    for (uint32_t i = 0; i < config->global.dns_server_count; i++)
    {
        inet_ntop(AF_INET, &config->global.dns_servers[i], ip_str, INET_ADDRSTRLEN);
        printf("    [%u] %s\n", i + 1, ip_str);
    }
    printf("\n");

    // NTP Servers
    if (config->global.ntp_server_count > 0)
    {
        printf("  NTP Servers (%u):\n", config->global.ntp_server_count);
        for (uint32_t i = 0; i < config->global.ntp_server_count; i++)
        {
            inet_ntop(AF_INET, &config->global.ntp_servers[i], ip_str, INET_ADDRSTRLEN);
            printf("    [%u] %s\n", i + 1, ip_str);
        }
        printf("\n");
    }

    // NetBIOS Servers
    if (config->global.netbios_server_count > 0)
    {
        printf("  NetBIOS Name Servers (%u):\n", config->global.netbios_server_count);
        for (uint32_t i = 0; i < config->global.netbios_server_count; i++)
        {
            inet_ntop(AF_INET, &config->global.netbios_servers[i], ip_str, INET_ADDRSTRLEN);
            printf("    [%u] %s\n", i + 1, ip_str);
        }
        printf("\n");
    }

    // Time Offset
    if (config->global.time_offset != 0)
    {
        printf("  Time Offset:              %d seconds (UTC%+f)\n",
               config->global.time_offset, config->global.time_offset / 3600.f);
        printf("\n");
    }

    // PXE Boot
    if (config->global.next_server.s_addr != 0 || strlen(config->global.filename) > 0)
    {
        printf("  PXE Boot Configuration:\n");
        if (config->global.next_server.s_addr != 0)
        {
            inet_ntop(AF_INET, &config->global.next_server, ip_str, INET_ADDRSTRLEN);
            printf("    Next Server:            %s\n", ip_str);
        }
        if (strlen(config->global.filename) > 0)
            printf("    Filename:               %s\n", config->global.filename);
        if (strlen(config->global.tftp_server_name) > 0)
            printf("    TFTP Server Name:       %s\n", config->global.tftp_server_name);
        if (strlen(config->global.bootfile_name) > 0)
            printf("    Bootfile Name:          %s\n", config->global.bootfile_name);
        printf("\n");
    }

    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                    SUBNET CONFIGURATIONS                       ║\n");
    printf("║                         Total: %-2u                            ║\n", config->subnet_count);
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");

    for (uint32_t i = 0; i < config->subnet_count; i++)
    {
        const struct dhcp_subnet_t *subnet = &config->subnets[i];

        printf("┌────────────────────────────────────────────────────────────────┐\n");
        inet_ntop(AF_INET, &subnet->network, ip_str, INET_ADDRSTRLEN);
        printf("│ Subnet #%u: %s\n", i + 1, ip_str);
        printf("├────────────────────────────────────────────────────────────────┤\n");

        // Network Configuration
        printf("  Network:\n");
        inet_ntop(AF_INET, &subnet->netmask, ip_str, INET_ADDRSTRLEN);
        printf("    Netmask:                %s\n", ip_str);
        inet_ntop(AF_INET, &subnet->range_start, ip_str, INET_ADDRSTRLEN);
        printf("    DHCP Range Start:       %s\n", ip_str);
        inet_ntop(AF_INET, &subnet->range_end, ip_str, INET_ADDRSTRLEN);
        printf("    DHCP Range End:         %s\n", ip_str);
        if (subnet->router.s_addr != 0)
        {
            inet_ntop(AF_INET, &subnet->router, ip_str, INET_ADDRSTRLEN);
            printf("    Default Gateway:        %s\n", ip_str);
        }
        if (subnet->broadcast.s_addr != 0)
        {
            inet_ntop(AF_INET, &subnet->broadcast, ip_str, INET_ADDRSTRLEN);
            printf("    Broadcast Address:      %s\n", ip_str);
        }
        if (subnet->subnet_mask.s_addr != 0)
        {
            inet_ntop(AF_INET, &subnet->subnet_mask, ip_str, INET_ADDRSTRLEN);
            printf("    Subnet Mask (option):   %s\n", ip_str);
        }
        if (strlen(subnet->domain_name) > 0)
            printf("    Domain Name:            %s\n", subnet->domain_name);
        printf("\n");

        // Lease Times
        printf("  Lease Configuration:\n");
        printf("    Default Lease:          %u seconds (%f hours)\n",
               subnet->default_lease_time, subnet->default_lease_time / 3600.f);
        printf("    Max Lease:              %u seconds (%f hours)\n",
               subnet->max_lease_time, subnet->max_lease_time / 3600.f);
        if (subnet->renewal_time > 0)
            printf("    Renewal Time (T1):      %u seconds\n", subnet->renewal_time);
        if (subnet->rebinding_time > 0)
            printf("    Rebinding Time (T2):    %u seconds\n", subnet->rebinding_time);
        printf("\n");

        // DNS Servers
        if (subnet->dns_server_count > 0)
        {
            printf("  DNS Servers (%u):\n", subnet->dns_server_count);
            for (uint32_t j = 0; j < subnet->dns_server_count; j++)
            {
                inet_ntop(AF_INET, &subnet->dns_servers[j], ip_str, INET_ADDRSTRLEN);
                printf("    [%u] %s\n", j + 1, ip_str);
            }
            printf("\n");
        }

        // NTP Servers
        if (subnet->ntp_server_count > 0)
        {
            printf("  NTP Servers (%u):\n", subnet->ntp_server_count);
            for (uint32_t j = 0; j < subnet->ntp_server_count; j++)
            {
                inet_ntop(AF_INET, &subnet->ntp_servers[j], ip_str, INET_ADDRSTRLEN);
                printf("    [%u] %s\n", j + 1, ip_str);
            }
            printf("\n");
        }

        // NetBIOS Servers
        if (subnet->netbios_server_count > 0)
        {
            printf("  NetBIOS Name Servers (%u):\n", subnet->netbios_server_count);
            for (uint32_t j = 0; j < subnet->netbios_server_count; j++)
            {
                inet_ntop(AF_INET, &subnet->netbios_servers[j], ip_str, INET_ADDRSTRLEN);
                printf("    [%u] %s\n", j + 1, ip_str);
            }
            printf("\n");
        }

        // Time Offset
        if (subnet->time_offset != 0)
        {
            printf("  Time Offset:              %d seconds (UTC%+f)\n",
                   subnet->time_offset, subnet->time_offset / 3600.f);
            printf("\n");
        }

        // PXE Boot
        if (subnet->next_server.s_addr != 0 || strlen(subnet->filename) > 0)
        {
            printf("  PXE Boot:\n");
            if (subnet->next_server.s_addr != 0)
            {
                inet_ntop(AF_INET, &subnet->next_server, ip_str, INET_ADDRSTRLEN);
                printf("    Next Server:            %s\n", ip_str);
            }
            if (strlen(subnet->filename) > 0)
                printf("    Filename:               %s\n", subnet->filename);
            if (strlen(subnet->tftp_server_name) > 0)
                printf("    TFTP Server:            %s\n", subnet->tftp_server_name);
            if (strlen(subnet->bootfile_name) > 0)
                printf("    Bootfile:               %s\n", subnet->bootfile_name);
            printf("\n");
        }

        // Host Reservations
        if (subnet->host_count > 0)
        {
            printf("  Host Reservations (%u):\n", subnet->host_count);
            for (uint32_t j = 0; j < subnet->host_count; j++)
            {
                inet_ntop(AF_INET, &subnet->hosts[j].fixed_address, ip_str, INET_ADDRSTRLEN);
                printf("    [%u] %-20s %s\n", j + 1, subnet->hosts[j].name, ip_str);
                printf("        MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                       subnet->hosts[j].mac_address[0],
                       subnet->hosts[j].mac_address[1],
                       subnet->hosts[j].mac_address[2],
                       subnet->hosts[j].mac_address[3],
                       subnet->hosts[j].mac_address[4],
                       subnet->hosts[j].mac_address[5]);
                if (strlen(subnet->hosts[j].hostname) > 0)
                    printf("  Hostname: %s", subnet->hosts[j].hostname);
                printf("\n");
            }
            printf("\n");
        }

        printf("└────────────────────────────────────────────────────────────────┘\n\n");
    }
}

void free_config(struct dhcp_config_t *config)
{
    memset(config, 0, sizeof(struct dhcp_config_t));
}