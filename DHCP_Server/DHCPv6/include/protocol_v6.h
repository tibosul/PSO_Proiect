#ifndef PROTOCOL_V6_H
#define PROTOCOL_V6_H


#include <stdint.h>
#include <netinet/in.h>
#include <sys/types.h>   /* ssize_t */
#include <stddef.h>      /* size_t */

/**
 * @file protocol_v6.h
 * @brief DHCPv6 protocol definitions and helpers (parsing + reply building).
 *
 * This module defines DHCPv6 message/option codes, packet header/option layouts,
 * a lightweight parsed-metadata structure, and helper APIs to parse requests and
 * build server replies (IA_NA and IA_PD).
 */


/* ===================== Ports ===================== */

/** DHCPv6 server UDP port (RFC 8415). */
#define DHCPV6_PORT_SERVER 547
/** DHCPv6 client UDP port (RFC 8415). */
#define DHCPV6_PORT_CLIENT 546

/* ===================== Message Types ===================== */

/** DHCPv6 Solicit message (RFC 8415). */
#define MSG_SOLICIT     1
/** DHCPv6 Advertise message (RFC 8415). */
#define MSG_ADVERTISE   2
/** DHCPv6 Request message (RFC 8415). */
#define MSG_REQUEST     3
/** DHCPv6 Confirm message (RFC 8415). */
#ifdef MSG_CONFIRM
#undef MSG_CONFIRM
#endif
#define MSG_CONFIRM     4
/** DHCPv6 Renew message (RFC 8415). */
#define MSG_RENEW       5
/** DHCPv6 Rebind message (RFC 8415). */
#define MSG_REBIND      6
/** DHCPv6 Reply message (RFC 8415). */
#define MSG_REPLY       7
/** DHCPv6 Release message (RFC 8415). */
#define MSG_RELEASE     8
/** DHCPv6 Decline message (RFC 8415). */
#define MSG_DECLINE     9
/** DHCPv6 Reconfigure message (RFC 8415). */
#define MSG_RECONFIGURE 10
/** DHCPv6 Information Request message (RFC 8415). */
#define MSG_INFO_REQ    11  

/* ===================== Option Codes ===================== */

/** DHCPv6 Client Identifier option (RFC 8415). */
#define OPT_CLIENTID    1
/** DHCPv6 Server Identifier option (RFC 8415). */
#define OPT_SERVERID    2
/** DHCPv6 IA_NA option (RFC 8415). */
#define OPT_IA_NA       3   
/** DHCPv6 IA Address option (RFC 8415). */
#define OPT_IAADDR      5   
/** DHCPv6 Option Request option (RFC 8415). */
#define OPT_ORO         6   
/** DHCPv6 Preference option (RFC 8415). */
#define OPT_PREFERENCE  7
/** DHCPv6 Elapsed Time option (RFC 8415). */
#define OPT_ELAPSED_TIME 8
/** DHCPv6 Relay Message option (RFC 8415). */
#define OPT_RELAY_MSG   9
/** DHCPv6 Status Code option (RFC 8415). */
#define OPT_STATUS_CODE 13
/** DHCPv6 DNS Servers option (RFC 8415). */
#define OPT_DNS_SERVERS 23
/** DHCPv6 IA_PD option (RFC 8415). */
#define OPT_IA_PD       25 
/** DHCPv6 IA Prefix option (RFC 8415). */
#define OPT_IAPREFIX    26 
/** DHCPv6 DNS Search List option (RFC 8415). */
#define OPT_DNS_SEARCH_LIST 24
/** DHCPv6 SNTP Servers option (RFC 8415). */
#define OPT_SNTP_SERVERS 31
/** DHCPv6 Information Refresh Time option (RFC 8415). */
#define OPT_INFO_REFRESH_TIME 32 

/* ===================== Status Codes ===================== */

/** DHCPv6 Success status code (RFC 8415). */
#define STATUS_SUCCESS      0
/** DHCPv6 Unspecified failure status code (RFC 8415). */
#define STATUS_UNSPECFAIL   1
/** DHCPv6 No address available status code (RFC 8415). */
#define STATUS_NOADDRSAVAIL 2
/** DHCPv6 No binding status code (RFC 8415). */
#define STATUS_NOBINDING    3
/** DHCPv6 Not on link status code (RFC 8415). */
#define STATUS_NOTONLINK    4
/** DHCPv6 Use multicast status code (RFC 8415). */
#define STATUS_USEMULTICAST 5
#pragma pack(push, 1)
/**
 * @brief DHCPv6 fixed header (4 bytes).
 *
 * Transaction ID is 24 bits split into xid_hi/xid_mid/xid_lo.
 */
typedef struct {
    uint8_t msg_type;   /**< DHCPv6 message type. */
    uint8_t xid_hi;     /**< DHCPv6 transaction ID (high byte). */
    uint8_t xid_mid;    /**< DHCPv6 transaction ID (middle byte). */
    uint8_t xid_lo;     /**< DHCPv6 transaction ID (low byte). */
} dhcpv6_header_t;
#pragma pack(pop)

#pragma pack(push, 1)
/**
 * @brief Generic DHCPv6 option header.
 *
 * The option is followed by a variable-size value buffer (value[]).
 * All fields are encoded in network byte order on the wire.
 */
typedef struct {
    uint16_t code;  /**< Option code (network order in packet). */
    uint16_t len;   /**< Option length (network order in packet). */
    uint8_t  value[]; /**< Option value bytes. */
} dhcpv6_option_t;

#pragma pack(pop)

/**
 * @brief Parsed metadata extracted from a DHCPv6 request.
 *
 * This struct stores pointers into the original packet buffer for DUIDs and other
 * variable-length fields (no deep copy). Keep the source buffer alive as long as
 * you use these pointers.
 *
 * Notes:
 * - client_duid/server_duid pointers refer to the option payload (raw DUID bytes).
 * - requested IP/prefix fields are best-effort extracted from IA_NA/IA_PD sub-options.
 * - DNS/SNTP/domain list fields are parsed only when those options are present.
 */
typedef struct {
    uint8_t msg_type;     /**< DHCPv6 message type. */
    uint32_t transaction_id;     /**< DHCPv6 transaction ID. */
    
    const uint8_t* client_duid;     /**< Client DUID (raw bytes). */
    uint16_t client_duid_len;       /**< Client DUID length. */
    
    const uint8_t* server_duid;     /**< Server DUID (raw bytes). */
    uint16_t server_duid_len;       /**< Server DUID length. */
    
    int has_ia_na;                  /**< Whether IA_NA option is present. */
    uint32_t iaid;                   /**< IA_NA IAID. */
    
    int has_requested_ip;           /**< Whether requested IP address is present. */
    struct in6_addr requested_ip;   /**< Requested IP address. */

    int has_ia_pd;                  /**< Whether IA_PD option is present. */
    uint32_t iaid_pd;               /**< IA_PD IAID. */
    int has_requested_prefix;       /**< Whether requested prefix is present. */
    struct in6_addr requested_prefix;   /**< Requested prefix. */
    uint8_t requested_plen;         /**< Requested prefix length. */

    // -- New Fields for Verification --
    struct in6_addr dns_servers[4];   /**< DNS servers (up to 4). */
    int dns_count;                    /**< Number of DNS servers. */
    
    struct in6_addr sntp_servers[4];  /**< SNTP servers (up to 4). */
    int sntp_count;                   /**< Number of SNTP servers. */
    
    uint8_t *domain_search;           /**< Domain search list (raw bytes). */
    uint16_t domain_search_len;       /**< Domain search list length. */

    uint32_t info_refresh_time;       /**< Information refresh time. */
    int has_info_refresh_time;        /**< Whether information refresh time is present. */

} dhcpv6_packet_meta_t;


/**
 * @brief Extract the 24-bit DHCPv6 Transaction ID from the on-wire header.
 * @param h Pointer to DHCPv6 header in packet buffer.
 * @return Transaction ID (lower 24 bits used).
 */
static inline uint32_t dhcpv6_get_xid(const dhcpv6_header_t *h)
{
    return ((uint32_t)h->xid_hi << 16) |
           ((uint32_t)h->xid_mid << 8) |
           (uint32_t)h->xid_lo;
}


/**
 * @brief Set the 24-bit DHCPv6 Transaction ID into the on-wire header.
 * @param h   Pointer to DHCPv6 header to be written.
 * @param xid Transaction ID (only lower 24 bits are used).
 */
static inline void dhcpv6_set_xid(dhcpv6_header_t *h, uint32_t xid)
{
    xid &= 0x00FFFFFFu;
    h->xid_hi  = (uint8_t)((xid >> 16) & 0xFF);
    h->xid_mid = (uint8_t)((xid >> 8)  & 0xFF);
    h->xid_lo  = (uint8_t)(xid & 0xFF);
}


/**
 * @brief Parse a DHCPv6 packet and fill a metadata structure.
 *
 * The parser scans the top-level options and extracts:
 * - ClientID and ServerID (DUID pointers)
 * - IA_NA (IAID and optional requested IAADDR)
 * - IA_PD (IAID and optional requested IAPREFIX)
 * - Optional DNS/SNTP/domain-search/info-refresh-time options (best-effort)
 *
 * Pointers stored in out_meta refer into the original buffer.
 *
 * @param buf      Input packet buffer (wire format).
 * @param len      Buffer length in bytes.
 * @param out_meta Output metadata structure to fill.
 * @return 0 on success, -1 on invalid input/truncated header.
 */
int dhcpv6_parse(const uint8_t *buf, size_t len, dhcpv6_packet_meta_t *out_meta);

/**
 * @brief Iterate through DHCPv6 options in a buffer.
 *
 * This function is used to traverse the options in a DHCPv6 message.
 * It returns a pointer to the next option in the buffer, or NULL if there are no more options.
 *
 * @param options    Buffer containing DHCPv6 options.
 * @param options_len Length of the options buffer in bytes.
 * @param last       Last option processed (NULL for first call).
 * @return Pointer to the next option, or NULL if no more options.
 */
const dhcpv6_option_t* dhcpv6_next_option(const uint8_t *options,
                                          size_t options_len,
                                          const dhcpv6_option_t *last);

/**
 * @brief Build a DHCPv6 IA_NA reply message.
 *
 * This function builds a DHCPv6 IA_NA reply message based on the provided request metadata.
 * It includes the IA_NA option with the assigned IP address and the requested IAADDR option.
 *
 * @param msg_type       DHCPv6 message type (e.g., DHCPv6_REPLY).
 * @param out_buf        Output buffer to store the built message.
 * @param out_buf_len    Length of the output buffer in bytes.
 * @param req_meta       Request metadata containing IA_NA and IAADDR information.
 * @param server_duid    Server DUID (raw bytes).
 * @param server_duid_len Length of the server DUID in bytes.
 * @param assigned_ip    Assigned IP address.
 * @param pref_lifetime  Preferred lifetime for the assigned IP address.
 * @param valid_lifetime Valid lifetime for the assigned IP address.
 * @return Number of bytes written to the output buffer, or -1 on error.
 */
ssize_t dhcpv6_build_ia_na_reply(uint8_t msg_type,
                                 uint8_t *out_buf,
                                 size_t out_buf_len,
                                 const dhcpv6_packet_meta_t *req_meta,
                                 const uint8_t *server_duid,
                                 uint16_t server_duid_len,
                                 const struct in6_addr *assigned_ip,
                                 uint32_t pref_lifetime,
                                 uint32_t valid_lifetime,
                                 uint16_t status_code /* STATUS_SUCCESS etc. */);
                    
/**
 * @brief Build a DHCPv6 IA_PD reply message.
 *
 * This function builds a DHCPv6 IA_PD reply message based on the provided request metadata.
 * It includes the IA_PD option with the delegated prefix and the requested IAPREFIX option.
 *
 * @param msg_type       DHCPv6 message type (e.g., DHCPv6_REPLY).
 * @param out_buf        Output buffer to store the built message.
 * @param out_buf_len    Length of the output buffer in bytes.
 * @param req_meta       Request metadata containing IA_PD and IAPREFIX information.
 * @param server_duid    Server DUID (raw bytes).
 * @param server_duid_len Length of the server DUID in bytes.
 * @param delegated_prefix Delegated prefix.
 * @param plen           Prefix length for the delegated prefix.
 * @param pref_lifetime  Preferred lifetime for the delegated prefix.
 * @param valid_lifetime Valid lifetime for the delegated prefix.
 * @return Number of bytes written to the output buffer, or -1 on error.
 */
ssize_t dhcpv6_build_ia_pd_reply(uint8_t msg_type,
                                 uint8_t *out_buf,
                                 size_t out_buf_len,
                                 const dhcpv6_packet_meta_t *req_meta,
                                 const uint8_t *server_duid,
                                 uint16_t server_duid_len,
                                 const struct in6_addr *delegated_prefix,
                                 uint8_t plen,
                                 uint32_t pref_lifetime,
                                 uint32_t valid_lifetime,
                                 uint16_t status_code);

/**
 * @brief Append a status code option to a DHCPv6 message.
 *
 * This function appends a status code option to the DHCPv6 message buffer.
 * It includes the status code and an optional message.
 *
 * @param buf        Output buffer to store the built message.
 * @param buf_len    Length of the output buffer in bytes.
 * @param status_code Status code to be appended.
 * @param message    Optional message to be appended (NULL for no message).
 * @return Number of bytes written to the output buffer, or -1 on error.
 */
ssize_t dhcpv6_append_status(uint8_t *buf, size_t buf_len,
                             uint16_t status_code,
                             const char *message);

/**
 * @brief Append an option to a DHCPv6 message.
 *
 * This function appends an option to the DHCPv6 message buffer.
 * It includes the option code and the option data.
 *
 * @param buf        Output buffer to store the built message.
 * @param buf_len    Length of the output buffer in bytes.
 * @param offset     Offset within the buffer where the option should be appended.
 * @param code       Option code to be appended.
 * @param data       Option data to be appended.
 * @param len        Length of the option data in bytes.
 * @return Number of bytes written to the output buffer, or -1 on error.
 */
ssize_t dhcpv6_append_option(uint8_t *buf, size_t buf_len, size_t *offset,
                             uint16_t code, const void *data, uint16_t len);

/**
 * @brief Append an IA_NA option to a DHCPv6 message.
 *
 * This function appends an IA_NA option to the DHCPv6 message buffer.
 * It includes the IA_NA information and an optional IAADDR option.
 *
 * @param buf        Output buffer to store the built message.
 * @param buf_len    Length of the output buffer in bytes.
 * @param offset     Offset within the buffer where the option should be appended.
 * @param iaid       IAID for the IA_NA option.
 * @param assigned_ip Assigned IP address for the IA_NA option.
 * @param t1         T1 value for the IA_NA option.
 * @param t2         T2 value for the IA_NA option.
 * @param pl         Preferred lifetime for the IA_NA option.
 * @param vl         Valid lifetime for the IA_NA option.
 * @param status_code Status code for the IA_NA option.
 * @return Number of bytes written to the output buffer, or -1 on error.
 */
ssize_t dhcpv6_append_ia_na(uint8_t *buf, size_t buf_len, size_t *offset,
                            uint32_t iaid, const struct in6_addr *assigned_ip,
                            uint32_t t1, uint32_t t2,
                            uint32_t pl, uint32_t vl, uint16_t status_code);

/**
 * @brief Append an IA_PD option to a DHCPv6 message.
 *
 * This function appends an IA_PD option to the DHCPv6 message buffer.
 * It includes the IA_PD information and an optional IAPREFIX option.
 *
 * @param buf        Output buffer to store the built message.
 * @param buf_len    Length of the output buffer in bytes.
 * @param offset     Offset within the buffer where the option should be appended.
 * @param iaid       IAID for the IA_PD option.
 * @param prefix     Delegated prefix for the IA_PD option.
 * @param plen       Prefix length for the IA_PD option.
 * @param t1         T1 value for the IA_PD option.
 * @param t2         T2 value for the IA_PD option.
 * @param pl         Preferred lifetime for the IA_PD option.
 * @param vl         Valid lifetime for the IA_PD option.
 * @param status_code Status code for the IA_PD option.
 * @return Number of bytes written to the output buffer, or -1 on error.
 */
ssize_t dhcpv6_append_ia_pd(uint8_t *buf, size_t buf_len, size_t *offset,
                            uint32_t iaid, const struct in6_addr *prefix, uint8_t plen,
                            uint32_t t1, uint32_t t2,
                            uint32_t pl, uint32_t vl, uint16_t status_code);

/**
 * @brief Append DNS server addresses to a DHCPv6 message.
 *
 * This function appends DNS server addresses to the DHCPv6 message buffer.
 * It includes the DNS server addresses and an optional domain search list.
 *
 * @param buf        Output buffer to store the built message.
 * @param buf_len    Length of the output buffer in bytes.
 * @param servers    Array of DNS server addresses.
 * @param count      Number of DNS server addresses.
 * @return Number of bytes written to the output buffer, or -1 on error.
 */
ssize_t dhcpv6_append_dns_servers(uint8_t *buf, size_t buf_len,
                                  const struct in6_addr *servers,
                                  size_t count);

/**
 * @brief Append a domain search list to a DHCPv6 message.
 *
 * This function appends a domain search list to the DHCPv6 message buffer.
 * It includes the domain search list and an optional domain name.
 *
 * @param buf        Output buffer to store the built message.
 * @param buf_len    Length of the output buffer in bytes.
 * @param domain_search_list Domain search list to be appended.
 * @return Number of bytes written to the output buffer, or -1 on error.
 */
ssize_t dhcpv6_append_domain_list(uint8_t *buf, size_t buf_len,
                                  const char *domain_search_list);

/**
 * @brief Append a 32-bit unsigned integer option to a DHCPv6 message.
 *
 * This function appends a 32-bit unsigned integer option to the DHCPv6 message buffer.
 * It includes the option code and the option data.
 *
 * @param buf        Output buffer to store the built message.
 * @param buf_len    Length of the output buffer in bytes.
 * @param code       Option code to be appended.
 * @param value      32-bit unsigned integer value to be appended.
 * @return Number of bytes written to the output buffer, or -1 on error.
 */
ssize_t dhcpv6_append_u32_option(uint8_t *buf, size_t buf_len,
                                 uint16_t code, uint32_t value);
#endif //PROTOCOL_V6_H