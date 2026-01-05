#ifndef PROTOCOL_V6_H
#define PROTOCOL_V6_H


#include <stdint.h>
#include <netinet/in.h>

#define DHCPV6_PORT_SERVER 547
#define DHCPV6_PORT_CLIENT 546

#define MSG_SOLICIT     1
#define MSG_ADVERTISE   2
#define MSG_REQUEST     3
#ifdef MSG_CONFIRM
#undef MSG_CONFIRM
#endif
#define MSG_CONFIRM     4
#define MSG_RENEW       5
#define MSG_REBIND      6
#define MSG_REPLY       7
#define MSG_RELEASE     8
#define MSG_DECLINE     9
#define MSG_RECONFIGURE 10
#define MSG_INFO_REQ    11

#define OPT_CLIENTID    1
#define OPT_SERVERID    2
#define OPT_IA_NA       3   
#define OPT_IAADDR      5   
#define OPT_ORO         6   
#define OPT_PREFERENCE  7
#define OPT_ELAPSED_TIME 8
#define OPT_RELAY_MSG   9
#define OPT_STATUS_CODE 13
#define OPT_DNS_SERVERS 23
#define OPT_IA_PD       25 
#define OPT_IAPREFIX    26 

#define STATUS_SUCCESS      0
#define STATUS_UNSPECFAIL   1
#define STATUS_NOADDRSAVAIL 2
#define STATUS_NOBINDING    3
#define STATUS_NOTONLINK    4
#define STATUS_USEMULTICAST 5
#pragma pack(push, 1)
typedef struct {
    uint8_t msg_type;
    uint8_t xid_hi;
    uint8_t xid_mid;
    uint8_t xid_lo;
} dhcpv6_header_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint16_t code;  
    uint16_t len;   
    uint8_t  value[]; 
} dhcpv6_option_t;

#pragma pack(pop)

typedef struct {
    uint8_t msg_type;
    uint32_t transaction_id; 
    
    const uint8_t* client_duid;
    uint16_t client_duid_len;
    
    const uint8_t* server_duid; 
    uint16_t server_duid_len;
    
    int has_ia_na;
    uint32_t iaid;       
    
    int has_requested_ip;
    struct in6_addr requested_ip; 

    int has_ia_pd;
    uint32_t iaid_pd;
    int has_requested_prefix;
    struct in6_addr requested_prefix;
    uint8_t requested_plen;

} dhcpv6_packet_meta_t;

static inline uint32_t dhcpv6_get_xid(const dhcpv6_header_t *h)
{
    return ((uint32_t)h->xid_hi << 16) |
           ((uint32_t)h->xid_mid << 8) |
           (uint32_t)h->xid_lo;
}

static inline void dhcpv6_set_xid(dhcpv6_header_t *h, uint32_t xid)
{
    xid &= 0x00FFFFFFu;
    h->xid_hi  = (uint8_t)((xid >> 16) & 0xFF);
    h->xid_mid = (uint8_t)((xid >> 8)  & 0xFF);
    h->xid_lo  = (uint8_t)(xid & 0xFF);
}

int dhcpv6_parse(const uint8_t *buf, size_t len, dhcpv6_packet_meta_t *out_meta);

const dhcpv6_option_t* dhcpv6_next_option(const uint8_t *options,
                                          size_t options_len,
                                          const dhcpv6_option_t *last);

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

ssize_t dhcpv6_append_status(uint8_t *buf, size_t buf_len,
                             uint16_t status_code,
                             const char *message);

ssize_t dhcpv6_append_option(uint8_t *buf, size_t buf_len, size_t *offset,
                             uint16_t code, const void *data, uint16_t len);

ssize_t dhcpv6_append_ia_na(uint8_t *buf, size_t buf_len, size_t *offset,
                            uint32_t iaid, const struct in6_addr *assigned_ip,
                            uint32_t t1, uint32_t t2,
                            uint32_t pl, uint32_t vl, uint16_t status_code);

ssize_t dhcpv6_append_ia_pd(uint8_t *buf, size_t buf_len, size_t *offset,
                            uint32_t iaid, const struct in6_addr *prefix, uint8_t plen,
                            uint32_t t1, uint32_t t2,
                            uint32_t pl, uint32_t vl, uint16_t status_code);

ssize_t dhcpv6_append_dns_servers(uint8_t *buf, size_t buf_len,
                                  const struct in6_addr *servers,
                                  size_t count);
#endif //PROTOCOL_V6_H