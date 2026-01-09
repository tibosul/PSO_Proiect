#include "protocol_v6.h"

#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif


static inline uint32_t get_u32(const uint8_t* p) {
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}

static inline void set_u32(uint8_t* p, uint32_t v) {
    memcpy(p, &v, 4);
}

const dhcpv6_option_t* dhcpv6_next_option(const uint8_t *options,
                                          size_t options_len,
                                          const dhcpv6_option_t *last)
{
    if (!options || options_len < sizeof(dhcpv6_option_t))
        return NULL;

    if (!last) {
        const dhcpv6_option_t *opt = (const dhcpv6_option_t *)options;
        if (options_len < sizeof(dhcpv6_option_t))
            return NULL;

        uint16_t len = ntohs(opt->len);
        if (sizeof(dhcpv6_option_t) + len > options_len)
            return NULL;

        return opt;
    }

    const uint8_t *base = options;
    const uint8_t *plast = (const uint8_t *)last;
    size_t off = (size_t)(plast - base);

    if (off >= options_len)
        return NULL;

    uint16_t last_len = ntohs(last->len);
    size_t next_off = off + sizeof(dhcpv6_option_t) + last_len;

    if (next_off + sizeof(dhcpv6_option_t) > options_len)
        return NULL;

    const dhcpv6_option_t *opt = (const dhcpv6_option_t *)(base + next_off);
    uint16_t len = ntohs(opt->len);
    if (next_off + sizeof(dhcpv6_option_t) + len > options_len)
        return NULL;

    return opt;
}

int dhcpv6_parse(const uint8_t *buf, size_t len, dhcpv6_packet_meta_t *out_meta)
{
    if (!buf || !out_meta)
        return -1;

    if (len < sizeof(dhcpv6_header_t))
        return -1;

    memset(out_meta, 0, sizeof(*out_meta));

    const dhcpv6_header_t *hdr = (const dhcpv6_header_t *)buf;
    out_meta->msg_type        = hdr->msg_type;
    out_meta->transaction_id  = dhcpv6_get_xid(hdr);

    size_t offset = sizeof(dhcpv6_header_t);

    while (offset + sizeof(dhcpv6_option_t) <= len) {
        const dhcpv6_option_t *opt =
            (const dhcpv6_option_t *)(buf + offset);

        uint16_t code    = ntohs(opt->code);
        uint16_t opt_len = ntohs(opt->len);
        size_t   opt_tot = sizeof(dhcpv6_option_t) + opt_len;

        if (offset + opt_tot > len)
            break; 

        const uint8_t *val = opt->value;

        switch (code) {
        case OPT_CLIENTID:
            out_meta->client_duid     = val;
            out_meta->client_duid_len = opt_len;
            break;

        case OPT_SERVERID:
            out_meta->server_duid     = val;
            out_meta->server_duid_len = opt_len;
            break;

        case OPT_IA_NA:
            if (opt_len >= 12) {
                out_meta->iaid      = ntohl(get_u32(val + 0));
                out_meta->has_ia_na = 1;

                size_t inner_off = 12; 
                while (inner_off + sizeof(dhcpv6_option_t) <= opt_len) {
                    const dhcpv6_option_t *sub =
                        (const dhcpv6_option_t *)(val + inner_off);
                    uint16_t scode = ntohs(sub->code);
                    uint16_t slen  = ntohs(sub->len);
                    size_t   stot  = sizeof(dhcpv6_option_t) + slen;

                    if (inner_off + stot > opt_len)
                        break;

                    if (scode == OPT_IAADDR && slen >= 16) {
                        memcpy(&out_meta->requested_ip, sub->value, 16);
                        out_meta->has_requested_ip = 1;
                    }

                    inner_off += stot;
                }
            }
            break;
        
        case OPT_IA_PD:
            if (opt_len >= 12) {
                out_meta->has_ia_pd = 1;
                out_meta->iaid_pd   = ntohl(get_u32(val + 0));
               
                const uint8_t *sub     = val + 12;
                size_t         sub_len = opt_len - 12;

                while (sub_len >= sizeof(dhcpv6_option_t)) {
                    const dhcpv6_option_t *sopt = (const dhcpv6_option_t *)sub;
                    uint16_t scode = ntohs(sopt->code);
                    uint16_t slen  = ntohs(sopt->len);
                    const uint8_t *sval = sopt->value;

                    if (sizeof(dhcpv6_option_t) + slen > sub_len)
                        break;

                    if (scode == OPT_IAPREFIX && slen >= (4 + 4 + 1 + 16)) {
                        out_meta->has_requested_prefix = 1;
                        out_meta->requested_plen       = sval[8];
                        memcpy(&out_meta->requested_prefix, sval + 9, 16);
                    }

                    size_t adv = sizeof(dhcpv6_option_t) + slen;
                    sub     += adv;
                    sub_len -= adv;
                }
            }
            break;

        case OPT_DNS_SERVERS:
            if (opt_len > 0 && opt_len % 16 == 0) {
                 int max_srv = sizeof(out_meta->dns_servers) / sizeof(out_meta->dns_servers[0]);
                 int count = opt_len / 16;
                 if (count > max_srv) count = max_srv;
                 
                 for(int i=0; i<count; i++) {
                     memcpy(&out_meta->dns_servers[i], val + i*16, 16);
                 }
                 out_meta->dns_count = count;
            }
            break;

        case OPT_DNS_SEARCH_LIST:
             if (opt_len > 0) {
                 out_meta->domain_search = (uint8_t*)val;
                 out_meta->domain_search_len = opt_len;
             }
             break;

        case OPT_SNTP_SERVERS:
            if (opt_len > 0 && opt_len % 16 == 0) {
                 int max_srv = sizeof(out_meta->sntp_servers) / sizeof(out_meta->sntp_servers[0]);
                 int count = opt_len / 16;
                 if (count > max_srv) count = max_srv;
                 
                 for(int i=0; i<count; i++) {
                     memcpy(&out_meta->sntp_servers[i], val + i*16, 16);
                 }
                 out_meta->sntp_count = count;
            }
            break;

        case OPT_INFO_REFRESH_TIME:
            if (opt_len >= 4) {
                 out_meta->info_refresh_time = ntohl(get_u32(val));
                 out_meta->has_info_refresh_time = 1;
            }
            break;

        default:
            break;
        }

        offset += opt_tot;
    }

    return 0;
}

ssize_t dhcpv6_append_status(uint8_t *buf, size_t buf_len,
                             uint16_t status_code,
                             const char *message)
{
    if (!buf || buf_len < sizeof(dhcpv6_option_t) + 2)
        return -1;

    size_t max_msg = 0;
    if (buf_len > sizeof(dhcpv6_option_t) + 2)
        max_msg = buf_len - (sizeof(dhcpv6_option_t) + 2);

    size_t msg_len = 0;
    if (message && max_msg > 0)
        msg_len = MIN(strlen(message), max_msg);

    uint16_t opt_len = (uint16_t)(2 + msg_len);
    size_t   total   = sizeof(dhcpv6_option_t) + opt_len;

    if (total > buf_len)
        return -1; 
    dhcpv6_option_t *opt = (dhcpv6_option_t *)buf;
    opt->code = htons(OPT_STATUS_CODE);
    opt->len  = htons(opt_len);

    uint8_t *val = opt->value;
    uint16_t sc  = htons(status_code);
    memcpy(val, &sc, 2);

    if (msg_len > 0)
        memcpy(val + 2, message, msg_len);

    return (ssize_t)total;
}

ssize_t dhcpv6_append_dns_servers(uint8_t *buf, size_t buf_len,
                                  const struct in6_addr *servers,
                                  size_t count)
{
    if (!buf || !servers || count == 0)
        return -1;

    if (buf_len < sizeof(dhcpv6_option_t) + 16)
        return -1;

    size_t max_servers = (buf_len - sizeof(dhcpv6_option_t)) / 16;
    if (max_servers == 0)
        return -1;

    size_t use = (count < max_servers) ? count : max_servers;
    uint16_t opt_len = (uint16_t)(use * 16);
    size_t total     = sizeof(dhcpv6_option_t) + opt_len;

    dhcpv6_option_t *opt = (dhcpv6_option_t *)buf;
    opt->code = htons(OPT_DNS_SERVERS);
    opt->len  = htons(opt_len);

    uint8_t *val = opt->value;
    for (size_t i = 0; i < use; i++) {
        memcpy(val + i * 16, &servers[i], 16);
    }

    return (ssize_t)total;
}


ssize_t dhcpv6_build_ia_na_reply(uint8_t msg_type,
                                 uint8_t *out_buf,
                                 size_t out_buf_len,
                                 const dhcpv6_packet_meta_t *req_meta,
                                 const uint8_t *server_duid,
                                 uint16_t server_duid_len,
                                 const struct in6_addr *assigned_ip,
                                 uint32_t pref_lifetime,
                                 uint32_t valid_lifetime,
                                 uint16_t status_code)
{
    if (!out_buf || out_buf_len < sizeof(dhcpv6_header_t) ||
        !req_meta || !server_duid || server_duid_len == 0 ||
        !assigned_ip)
        return -1;

    size_t pos = 0;

    dhcpv6_header_t *hdr = (dhcpv6_header_t *)out_buf;
    hdr->msg_type = msg_type;
    dhcpv6_set_xid(hdr, req_meta->transaction_id);
    pos += sizeof(dhcpv6_header_t);

    if (pos + sizeof(dhcpv6_option_t) + server_duid_len > out_buf_len) return -1;
    dhcpv6_option_t *opt_srv = (dhcpv6_option_t *)(out_buf + pos);
    opt_srv->code = htons(OPT_SERVERID);
    opt_srv->len  = htons(server_duid_len);
    memcpy(opt_srv->value, server_duid, server_duid_len);
    pos += sizeof(dhcpv6_option_t) + server_duid_len;

    if (req_meta->client_duid && req_meta->client_duid_len > 0) {
        if (pos + sizeof(dhcpv6_option_t) + req_meta->client_duid_len > out_buf_len) return -1;
        dhcpv6_option_t *opt_cli = (dhcpv6_option_t *)(out_buf + pos);
        opt_cli->code = htons(OPT_CLIENTID);
        opt_cli->len  = htons(req_meta->client_duid_len);
        memcpy(opt_cli->value, req_meta->client_duid, req_meta->client_duid_len);
        pos += sizeof(dhcpv6_option_t) + req_meta->client_duid_len;
    }

    if (!req_meta->has_ia_na) {
        return (ssize_t)pos;
    }

    uint8_t ia_na_val[512];
    size_t  ia_len = 0;

    set_u32(ia_na_val + ia_len, htonl(req_meta->iaid));
    ia_len += 4;

    set_u32(ia_na_val + ia_len, htonl(pref_lifetime / 2));
    ia_len += 4;
    set_u32(ia_na_val + ia_len, htonl(pref_lifetime));
    ia_len += 4;

    if (ia_len + sizeof(dhcpv6_option_t) + 16 + 4 + 4 > sizeof(ia_na_val))
        return -1;

    dhcpv6_option_t *iaaddr_opt = (dhcpv6_option_t *)(ia_na_val + ia_len);
    iaaddr_opt->code = htons(OPT_IAADDR);
    iaaddr_opt->len  = htons(16 + 4 + 4);

    uint8_t *iaaddr_val = iaaddr_opt->value;
    memcpy(iaaddr_val, assigned_ip, 16);
    set_u32(iaaddr_val + 16, htonl(pref_lifetime));
    set_u32(iaaddr_val + 20, htonl(valid_lifetime));
    ia_len += sizeof(dhcpv6_option_t) + 16 + 4 + 4;

    if (status_code != STATUS_SUCCESS) {
        // Simple status
        ssize_t s = dhcpv6_append_status(ia_na_val + ia_len,
                                         sizeof(ia_na_val) - ia_len,
                                         status_code,
                                         "Error");
        if (s < 0) return -1;
        ia_len += (size_t)s;
    }

    if (pos + sizeof(dhcpv6_option_t) + ia_len > out_buf_len) return -1;
    dhcpv6_option_t *final_opt = (dhcpv6_option_t *)(out_buf + pos);
    final_opt->code = htons(OPT_IA_NA);
    final_opt->len = htons((uint16_t)ia_len);
    memcpy(final_opt->value, ia_na_val, ia_len);
    pos += sizeof(dhcpv6_option_t) + ia_len;

    return (ssize_t)pos;
}

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
                                 uint16_t status_code)
{
    if (!out_buf || !req_meta) return -1;

    uint8_t *p      = out_buf;
    size_t   remain = out_buf_len;

    if (remain < sizeof(dhcpv6_header_t)) return -1;

    dhcpv6_header_t *hdr = (dhcpv6_header_t *)p;
    hdr->msg_type = msg_type;
    dhcpv6_set_xid(hdr, req_meta->transaction_id);

    p      += sizeof(dhcpv6_header_t);
    remain -= sizeof(dhcpv6_header_t);

    // Server ID
    if (server_duid && server_duid_len) {
        if (remain < sizeof(dhcpv6_option_t) + server_duid_len) return -1;
        dhcpv6_option_t *opt = (dhcpv6_option_t *)p;
        opt->code = htons(OPT_SERVERID);
        opt->len  = htons(server_duid_len);
        memcpy(opt->value, server_duid, server_duid_len);
        p      += sizeof(dhcpv6_option_t) + server_duid_len;
        remain -= sizeof(dhcpv6_option_t) + server_duid_len;
    }

    // Client ID
    if (req_meta->client_duid && req_meta->client_duid_len) {
        if (remain < sizeof(dhcpv6_option_t) + req_meta->client_duid_len) return -1;
        dhcpv6_option_t *opt = (dhcpv6_option_t *)p;
        opt->code = htons(OPT_CLIENTID);
        opt->len  = htons(req_meta->client_duid_len);
        memcpy(opt->value, req_meta->client_duid, req_meta->client_duid_len);
        p      += sizeof(dhcpv6_option_t) + req_meta->client_duid_len;
        remain -= sizeof(dhcpv6_option_t) + req_meta->client_duid_len;
    }

    // IA_PD
    {
        size_t ia_body_len = 12 + (sizeof(dhcpv6_option_t) + 4 + 4 + 1 + 16);
        if (remain < sizeof(dhcpv6_option_t) + ia_body_len) return -1;

        dhcpv6_option_t *opt_ia = (dhcpv6_option_t *)p;
        opt_ia->code = htons(OPT_IA_PD);
        opt_ia->len  = htons((uint16_t)ia_body_len);
        uint8_t *ia_body = opt_ia->value;

        uint32_t iaid = req_meta->has_ia_pd ? req_meta->iaid_pd : 0;
        set_u32(ia_body + 0, htonl(iaid));
        set_u32(ia_body + 4, 0); 
        set_u32(ia_body + 8, 0);

        uint8_t *sub = ia_body + 12;

        dhcpv6_option_t *opt_pfx = (dhcpv6_option_t *)sub;
        opt_pfx->code = htons(OPT_IAPREFIX);
        opt_pfx->len  = htons(4 + 4 + 1 + 16);

        uint8_t *pb = opt_pfx->value;
        set_u32(pb + 0, htonl(pref_lifetime));
        set_u32(pb + 4, htonl(valid_lifetime));
        pb[8]                 = plen;
        if (delegated_prefix)
            memcpy(pb + 9, delegated_prefix, 16);
        else
            memset(pb + 9, 0, 16);

        p      += sizeof(dhcpv6_option_t) + ia_body_len;
        remain -= sizeof(dhcpv6_option_t) + ia_body_len;
    }

    return (ssize_t)(p - out_buf);
}

ssize_t dhcpv6_append_option(uint8_t *buf, size_t buf_len, size_t *offset,
                             uint16_t code, const void *data, uint16_t len)
{
    if (!buf || !offset) return -1;
    size_t pos = *offset;
    if (pos + sizeof(dhcpv6_option_t) + len > buf_len) return -1;
    
    dhcpv6_option_t *opt = (dhcpv6_option_t *)(buf + pos);
    opt->code = htons(code);
    opt->len  = htons(len);
    if (data && len > 0)
        memcpy(opt->value, data, len);
        
    *offset = pos + sizeof(dhcpv6_option_t) + len;
    return (ssize_t)(*offset);
}

ssize_t dhcpv6_append_ia_na(uint8_t *buf, size_t buf_len, size_t *offset,
                            uint32_t iaid, const struct in6_addr *assigned_ip,
                            uint32_t t1, uint32_t t2,
                            uint32_t pl, uint32_t vl, uint16_t status_code)
{
    if (!buf || !offset) return -1;
    
    size_t pos = *offset;
    if (pos + sizeof(dhcpv6_option_t) + 12 > buf_len) return -1;
    
    dhcpv6_option_t *opt_ia = (dhcpv6_option_t *)(buf + pos);
    opt_ia->code = htons(OPT_IA_NA);
    uint8_t *val = opt_ia->value;
    
    set_u32(val + 0, htonl(iaid));
    set_u32(val + 4, htonl(t1));
    set_u32(val + 8, htonl(t2));
    
    size_t ia_len = 12; 
    size_t sub_off = pos + sizeof(dhcpv6_option_t) + 12; 
    
    if (status_code == STATUS_SUCCESS) {
        if (sub_off + sizeof(dhcpv6_option_t) + 24 > buf_len) return -1;
        
        dhcpv6_option_t *opt_addr = (dhcpv6_option_t *)(buf + sub_off);
        opt_addr->code = htons(OPT_IAADDR);
        opt_addr->len  = htons(24);
        uint8_t *aval = opt_addr->value;
        memcpy(aval, assigned_ip, 16);
        set_u32(aval + 16, htonl(pl));
        set_u32(aval + 20, htonl(vl));
        
        ia_len += sizeof(dhcpv6_option_t) + 24;
    } else {
        const char* msg = "Error";
        size_t msg_len = strlen(msg);
        size_t sc_len = 2 + msg_len;
        
        if (sub_off + sizeof(dhcpv6_option_t) + sc_len > buf_len) return -1;
        dhcpv6_option_t *opt_st = (dhcpv6_option_t *)(buf + sub_off);
        opt_st->code = htons(OPT_STATUS_CODE);
        opt_st->len = htons((uint16_t)sc_len);
        uint16_t sc = htons(status_code);
        memcpy(opt_st->value, &sc, 2);
        memcpy(opt_st->value + 2, msg, msg_len);
        
        ia_len += sizeof(dhcpv6_option_t) + sc_len;
    }
    
    opt_ia->len = htons((uint16_t)ia_len);
    *offset = pos + sizeof(dhcpv6_option_t) + ia_len;
    return (ssize_t)(*offset);
}

ssize_t dhcpv6_append_ia_pd(uint8_t *buf, size_t buf_len, size_t *offset,
                            uint32_t iaid, const struct in6_addr *prefix, uint8_t plen,
                            uint32_t t1, uint32_t t2,
                            uint32_t pl, uint32_t vl, uint16_t status_code)
{
    if (!buf || !offset) return -1;
    
    size_t pos = *offset;
    if (pos + sizeof(dhcpv6_option_t) + 12 > buf_len) return -1;
    
    dhcpv6_option_t *opt_pd = (dhcpv6_option_t *)(buf + pos);
    opt_pd->code = htons(OPT_IA_PD);
    uint8_t *val = opt_pd->value;
    
    set_u32(val + 0, htonl(iaid));
    set_u32(val + 4, htonl(t1));
    set_u32(val + 8, htonl(t2));
    
    size_t ia_len = 12;
    size_t sub_off = pos + sizeof(dhcpv6_option_t) + 12;
    
    if (status_code == STATUS_SUCCESS) {
        if (sub_off + sizeof(dhcpv6_option_t) + 25 > buf_len) return -1;
        
        dhcpv6_option_t *opt_pfx = (dhcpv6_option_t *)(buf + sub_off);
        opt_pfx->code = htons(OPT_IAPREFIX);
        opt_pfx->len = htons(25);
        uint8_t *pval = opt_pfx->value;
        set_u32(pval + 0, htonl(pl));
        set_u32(pval + 4, htonl(vl));
        pval[8] = plen;
        if (prefix) memcpy(pval + 9, prefix, 16);
        else memset(pval + 9, 0, 16);
        
        ia_len += sizeof(dhcpv6_option_t) + 25;
    } else {
        const char* msg = "Error";
        size_t msg_len = strlen(msg);
        size_t sc_len = 2 + msg_len;
        
        if (sub_off + sizeof(dhcpv6_option_t) + sc_len > buf_len) return -1;
        dhcpv6_option_t *opt_st = (dhcpv6_option_t *)(buf + sub_off);
        opt_st->code = htons(OPT_STATUS_CODE);
        opt_st->len = htons((uint16_t)sc_len);
        uint16_t sc = htons(status_code);
        memcpy(opt_st->value, &sc, 2);
        memcpy(opt_st->value + 2, msg, msg_len);
        
        ia_len += sizeof(dhcpv6_option_t) + sc_len;
    }
    
    opt_pd->len = htons((uint16_t)ia_len);
    *offset = pos + sizeof(dhcpv6_option_t) + ia_len;
    return (ssize_t)(*offset);
}


ssize_t dhcpv6_append_domain_list(uint8_t *buf, size_t buf_len, const char *domain_search_list)
{
    if (!buf || !domain_search_list) return -1;

    // Estimate max size needed (input length + some overhead)
    size_t needed = strlen(domain_search_list) + 256; 
    if (buf_len < sizeof(dhcpv6_option_t) + needed) return -1;

    dhcpv6_option_t *opt = (dhcpv6_option_t *)buf;
    opt->code = htons(OPT_DNS_SEARCH_LIST);
    
    uint8_t *val = opt->value;
    size_t pos = 0;
    
    // Parse comma-separated list
    char tmp[512];
    strncpy(tmp, domain_search_list, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';
    
    char *saveptr;
    char *token = strtok_r(tmp, ",", &saveptr);
    while (token) {
        // Trim
        char *d = token;
        while (*d && (*d == ' ' || *d == '\t')) d++;
        size_t len_d = strlen(d);
        while (len_d > 0 && (d[len_d-1] == ' ' || d[len_d-1] == '\t' || d[len_d-1] == '\n')) {
            d[len_d-1] = '\0';
            len_d--;
        }

        if (*d) {
             extern int encode_domain_name(const char *domain, uint8_t *buf, size_t len);
             int n = encode_domain_name(d, val + pos, buf_len - sizeof(dhcpv6_option_t) - pos);
             if (n < 0) return -1;
             pos += (size_t)n;
        }
        token = strtok_r(NULL, ",", &saveptr);
    }

    opt->len = htons((uint16_t)pos);
    return (ssize_t)(sizeof(dhcpv6_option_t) + pos);
}

ssize_t dhcpv6_append_u32_option(uint8_t *buf, size_t buf_len, uint16_t code, uint32_t value)
{
    if (!buf) return -1;
    if (buf_len < sizeof(dhcpv6_option_t) + 4) return -1;

    dhcpv6_option_t *opt = (dhcpv6_option_t *)buf;
    opt->code = htons(code);
    opt->len  = htons(4);
    
    uint32_t v_net = htonl(value);
    memcpy(opt->value, &v_net, 4);

    return (ssize_t)(sizeof(dhcpv6_option_t) + 4);
}