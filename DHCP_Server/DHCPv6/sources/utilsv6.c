#include "utilsv6.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


bool is_valid_ipv6(const char *ip_str)
{
    struct in6_addr tmp;
    if(!ip_str) return false;

    return inet_pton(AF_INET6, ip_str, &tmp) == 1;
}


int ipv6_str_to_bin(const char *ip_str, struct in6_addr *addr)
{
    if(!ip_str || !addr) return -1;

    if(inet_pton(AF_INET6, ip_str, addr)!=1)
    {
        return -1;
    }
    return 0;
}

int ipv6_bin_to_str(const struct in6_addr *addr, char *ip_str, size_t size)
{
    if(!addr || !ip_str || size == 0) return -1;

    const char *res = inet_ntop(AF_INET6, addr, ip_str, size);

    return (res == NULL) ? -1 : 0;
}

int ipv6_compare(const struct in6_addr *addr1, const struct in6_addr *addr2)
{
    return memcmp(addr1, addr2, sizeof(struct in6_addr));
}

bool ipv6_in_subnet(const struct in6_addr *ip, const struct in6_addr *subnet, uint8_t prefix_len)
{
    if(!ip || !subnet) return false;

    if(prefix_len > 128) return false;

    uint8_t full_bytes = prefix_len / 8;
    uint8_t remaining_bits = prefix_len % 8;

    if(full_bytes > 0)
    {
         if(memcmp(ip->s6_addr, subnet->s6_addr, full_bytes) != 0)
         {
             return false;
         }
    }

    if(remaining_bits != 0)
    {
        uint8_t mask = (uint8_t) (0xFF << (8-remaining_bits));
        uint8_t ip_part = ip->s6_addr[full_bytes] & mask;
        uint8_t subnet_part = subnet->s6_addr[full_bytes] & mask;
        if(ip_part != subnet_part)
        {
            return false;
        }

    }
    return true;
}

bool ipv6_increment(struct in6_addr *ip)
{
    if(!ip) return false;

    for(int i = 15;i>=0;i--)
    {
        if(ip->s6_addr[i]!=0xFF)
        {
            ip->s6_addr[i]+=1;
            return true;

        }
        ip->s6_addr[i]=0x00;
    }
    return false;
}


bool is_valid_mac(const char *mac_str)
{
    if(!mac_str) return false;

    int len = (int) strlen(mac_str);
    if(len!=17)
        return false;

    for(int i=0;i<len;i++)
    {
        if((i%3)==2)
        {
            //separatoare
            if(mac_str[i]!=':')
            {
                return false;
            }
        }
        else{
            //hexa
            if(!isxdigit((unsigned char)mac_str[i]))
            {
                return false;
            }
        }
    }
    return true;
}

bool is_valid_duid(const char *duid_str)
{
    if(!duid_str) return false;

    int hex_count = 0;

    for(int i=0;duid_str[i]!='\0';i++)
    {
        char c = duid_str[i];
        if(c == ':')
        {
            continue;
        }

        if(!isxdigit((unsigned char)c))
        {
            return false;
        }
        hex_count++;
    }

    if(hex_count<8)
    {
        return false;
    }

    if((hex_count%2)!=0)
    {
        return false;
    }
    return true;
}

void rstrip(char *str)
{
    int len = (int)strlen(str);
    while(len > 0 &&(str[len-1]=='\n' || str[len-1]=='\r'))
    {
        str[len-1]='\0';
        len--;
    }
}

char *lskip(char *s)
{
    while(*s &&(*s ==' ' || *s=='\t'))
    {
        s++;
    }
    return s;
}

void strip_semicolon(char *s)
{
    rstrip(s);
    int len = (int) strlen(s);
    while(len > 0 && (s[len-1]==';' || s[len-1]==' ' || s[len-1]=='\t'))
    {
        s[len-1]='\0';
        len--;
    }
}

void unquote(char *s)
{
    size_t len=strlen(s);
    if(len>=2 && s[0]=='"' && s[len-1]=='"')
    {
        memmove(s,s+1,len-2);
        s[len-2]='\0';
    }
}

void strip_inline_comment(char *s) {
    int in_quote = 0;
    for (char *p = s; *p; ++p) {
        if (*p == '"' && (p == s || *(p-1) != '\\')) {
            in_quote = !in_quote;
            continue;
        }
        if (!in_quote && *p == '#') {
            *p = '\0';
            break;
        }
    }
    rstrip(s);
}

int parse_prefix_and_len(const char *in, char *prefix_out,size_t prefix_out_sz,uint8_t *len_out)
{
    const char *slash = strchr(in,'/');
    if(!slash) return -1;

    size_t addr_len = (size_t) (slash - in);
    if(addr_len >= prefix_out_sz) return -1;

    memcpy(prefix_out,in,addr_len);
    prefix_out[addr_len]='\0';

    long pref_len = strtol(slash+1,NULL,10);
    if(pref_len < 0 || pref_len > 128) return -1;

    *len_out=(uint8_t)pref_len;
    return 0;
}
int duid_bin_to_hex(const uint8_t* in, uint16_t len, char* out, size_t out_sz)
{
    if (!out || out_sz == 0) return -1;
    if (!in && len != 0) { out[0] = '\0'; return -1; }

    
    size_t need = len ? (size_t)(len * 3 - 1) + 1 : 1;
    if (out_sz < need) { out[0] = '\0'; return -1; }

    static const char* h = "0123456789abcdef";
    size_t pos = 0;

    for (uint16_t i = 0; i < len; i++) {
        out[pos++] = h[(in[i] >> 4) & 0xF];
        out[pos++] = h[in[i] & 0xF];
        if (i + 1 < len) {
            out[pos++] = ':';     
        }
    }
    out[pos] = '\0';
    return (int)pos;
}
