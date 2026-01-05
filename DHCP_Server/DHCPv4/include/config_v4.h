#ifndef CONFIG_V4_H
#define CONFIG_V4_H

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>

#define MAX_SUBNETS 32
#define MAX_HOSTS_PER_SUBNET 128
#define MAX_DNS_SERVERS 4
#define MAX_NTP_SERVERS 4
#define MAX_NETBIOS_SERVERS 4
#define MAX_HOSTNAME_LENGTH 256
#define MAX_DOMAIN_LENGTH 256

typedef enum
{
    DDNS_NONE = 0,
    DDNS_INTERIM,
    DDNS_STANDARD,
    DDNS_AD_HOC,
    DDNS_UNKNOWN
} ddns_update_style_t;

/**
 * @brief Convert DDNS update style enum to string.
 * @param style DDNS update style enumeration value.
 * @return String representation (e.g., "none", "interim").
 */
const char *ddns_update_style_to_string(ddns_update_style_t style);

/**
 * @brief Convert string to DDNS update style enum.
 * @param str DDNS style string (e.g., "none", "interim").
 * @return DDNS update style enumeration value.
 */
ddns_update_style_t ddns_update_style_from_string(const char *str);

struct dhcp_global_options_t
{
    bool authoritative;    // false by default
    bool ping_check;       // whether to ping the client before giving an address
    uint32_t ping_timeout; // in seconds
    ddns_update_style_t ddns_update_style;

    uint32_t default_lease_time; // in seconds
    uint32_t max_lease_time;     // in seconds

    // Global time and server options (can be overridden per subnet)
    struct in_addr dns_servers[MAX_DNS_SERVERS];
    uint32_t dns_server_count;                           // number of DNS servers
    struct in_addr ntp_servers[MAX_NTP_SERVERS];         // DHCP option 42 - NTP servers
    uint32_t ntp_server_count;                           // number of NTP servers
    struct in_addr netbios_servers[MAX_NETBIOS_SERVERS]; // DHCP option 44 - NetBIOS name servers
    uint32_t netbios_server_count;                       // number of NetBIOS servers
    int32_t time_offset;                                 // DHCP option 2 - Time offset from UTC (seconds)

    // PXE Boot support (network booting)
    struct in_addr next_server; // Boot server IP address
    char filename[256];         // Boot file name (e.g., "pxelinux.0")
    char tftp_server_name[256]; // DHCP option 66 - TFTP server name/IP
    char bootfile_name[256];    // DHCP option 67 - Boot file name

    // Lease renewal timers (T1 and T2)
    uint32_t renewal_time;   // DHCP option 58 (T1) - time until RENEWING state (seconds)
    uint32_t rebinding_time; // DHCP option 59 (T2) - time until REBINDING state (seconds)

    // Server behavior control
    bool allow_unknown_clients; // Allow clients without reservations (default: true)
    bool allow_bootp;           // Allow BOOTP requests (default: true)

    bool update_conflict_detection; // false by default
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
    struct in_addr ntp_servers[MAX_NTP_SERVERS];
    uint32_t ntp_server_count;
    struct in_addr netbios_servers[MAX_NTP_SERVERS];
    uint32_t netbios_server_count;
    int32_t time_offset;

    // Subnet-specific lease times (optional overrides)
    uint32_t default_lease_time; // 0 means use global
    uint32_t max_lease_time;     // 0 means use global

    // PXE Boot support (subnet-level overrides)
    struct in_addr next_server; // Boot server IP (0.0.0.0 means use global)
    char filename[256];         // Boot file name (empty means use global)
    char tftp_server_name[256]; // DHCP option 66 (empty means use global)
    char bootfile_name[256];    // DHCP option 67 (empty means use global)

    // Lease renewal timers (subnet-level overrides)
    uint32_t renewal_time;   // DHCP option 58 (0 means use global)
    uint32_t rebinding_time; // DHCP option 59 (0 means use global)

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

// -----------------------------------------------------------------

/**
 * @brief Parse a DHCPv4 configuration file.
 * @param filename Path to the configuration file.
 * @param config Pointer to dhcp_config_t structure to populate.
 * @return 0 on success,
 *         -1 if filename or config is NULL, or file cannot be opened,
 *         -2 if parsing fails for any configuration option
 */
int parse_config_file(const char *filename, struct dhcp_config_t *config);

/** @brief Free memory allocated for the configuration.
 *  @param config Pointer to dhcp_config_t structure to free.
 */
void free_config(struct dhcp_config_t *config);

/**
 * @brief Print the DHCPv4 configuration to stdout.
 * @param config Pointer to dhcp_config_t structure to print.
 */
void print_config(const struct dhcp_config_t *config);

/**
 * @brief Find the subnet that contains the given IP address.
 * @param config Pointer to dhcp_config_t structure.
 * @param ip IP address to search for.
 * @return Pointer to dhcp_subnet_t if found,
 *         NULL if config is NULL or no matching subnet is found
 */
struct dhcp_subnet_t *find_subnet_for_ip(const struct dhcp_config_t *config, const struct in_addr ip);

/**
 * @brief Find a host reservation by MAC address within a subnet.
 * @param subnet Pointer to dhcp_subnet_t structure.
 * @param mac MAC address to search for (6 bytes).
 * @return Pointer to dhcp_host_reservation_t if found,
 *         NULL if subnet or mac is NULL, or no matching host is found
 */
struct dhcp_host_reservation_t *find_host_by_mac(const struct dhcp_subnet_t *subnet, const uint8_t mac[6]);

#endif // CONFIG_V4_H