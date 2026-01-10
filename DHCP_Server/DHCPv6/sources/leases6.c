#include "leases6.h"
#include "logger.h"
#include "utilsv6.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define READ_BUF_SZ (1<<16)
#define WR_TMP_MAX 8192

typedef struct
{
    char buf[READ_BUF_SZ];
    size_t len;
    size_t pos;
    int fd;
}rd_ctx_t;

static inline void in6_to_str(const struct in6_addr* a, char* out, size_t out_size)
{
    if(!out || out_size==0) return;
    if(!inet_ntop(AF_INET6,a,out,out_size)) {if(out_size) out[0]='\0';}
}

static inline int str_to_in6(const char* s, struct in6_addr* adress){return inet_pton(AF_INET6,s,adress)==1 ? 0:-1;}

static char* ltrim(char* s){while(isspace((unsigned char)*s))s++; return s;}

static void rtrim_inplace(char* s){
    size_t n = strlen(s);
    while(n>0 && isspace((unsigned char)s[n-1]))
    s[--n]='\0';
}

static char* trim(char* s){return rtrim_inplace(ltrim(s)),s;}

static time_t parse_lease_time_any(const char* s)
{
    if (!s) return 0;

    // 1) epoch numeric: "starts %lld;"
    {
        long long t = 0;
        if (sscanf(s, "%lld", &t) == 1 && t > 0) {
            return (time_t)t;
        }
    }

    // 2) "w YYYY/MM/DD HH:MM:SS"
    {
        int w = 0;
        char buf[64];
        if (sscanf(s, "%d %63[^\n]", &w, buf) == 2) {
            struct tm tmv;
            memset(&tmv, 0, sizeof(tmv));
            // acceptă exact "YYYY/MM/DD HH:MM:SS"
            if (strptime(buf, "%Y/%m/%d %H:%M:%S", &tmv) != NULL) {
                tmv.tm_isdst = -1;               
                time_t t = mktime(&tmv);         
                if (t > 0) return t;
            }
        }
    }

    // 3) "YYYY/MM/DD HH:MM:SS" (fără zi)
    {
        struct tm tmv;
        memset(&tmv, 0, sizeof(tmv));
        if (strptime(s, "%Y/%m/%d %H:%M:%S", &tmv) != NULL) {
            tmv.tm_isdst = -1;
            time_t t = mktime(&tmv);
            if (t > 0) return t;
        }
    }

    return 0;
}
static ssize_t write_all(int fd, const void* buf, size_t len)
{
    const uint8_t* p =(const uint8_t*)buf;
    size_t left=len;

    while(left>0)
    {
        ssize_t w = write(fd,p,left);
        if(w<0)
        {
            if(errno == EINTR)
                continue;
            return -1;
        }
        p += w;
        left-=(size_t)w;
    }
    return (ssize_t)len;
}

static int write_fmt(int fd, const char* fmt,...)
{
    char small[WR_TMP_MAX];
    va_list ap;
    va_start(ap,fmt);
    int n = vsnprintf(small,sizeof(small),fmt,ap);
    va_end(ap);

    if(n<0) return -1;

    size_t to_write = (n < (int)sizeof(small)) ? (size_t)n : (sizeof(small) - 1);
    return (write_all(fd, small, to_write) < 0) ? -1 : 0;
}

static int fsync_dirname(const char* path)
{
    char tmp[LEASE6_PATH_MAX];
    strncpy(tmp,path,sizeof(tmp)-1);
    tmp[sizeof(tmp)-1]='\0';
    char *slash=strrchr(tmp,'/');
    int fd = -1;
    if (slash) {
         *slash = '\0';
         fd = open(tmp, O_RDONLY | O_DIRECTORY);
         if (fd >= 0) {
             (void)fsync(fd);
             close(fd);
        }
    }
    return 0;
}


static int duid_hex_to_bin(const char* hex, uint8_t *out, uint16_t out_max)
{
    if(!hex || !out || out_max==0) return -1;
    uint16_t n=0;
    const char *p=hex;
    while(*p)
    {
        if(*p==':' || *p==' ' || *p=='\t') p++;
        if(!*p)break;
        unsigned int byte;
        if(sscanf(p,"%2x",&byte)!=1) return -1;
        if(n>=out_max) return -1;
        out[n++] = (uint8_t)byte;
        const char *colon = strchr(p,':');

        if(!colon)
        {
            const char *q=p;
            int nd=0;
            while(*q && *q!=':' && !isspace((unsigned char)*q))
            {
                q++;
                nd++;
            }
            if(nd>2) return -1;
            break;
        }
        p = colon+1;
    }
    return (int)n;
}

// static int duid_bin_to_hex(const uint8_t *duid, uint16_t duid_len, char *out,size_t outsz)
// {
//     if(!out || outsz ==0) return -1;
//     if(!duid && duid_len!=0) return -1;
//     size_t need=(duid_len ? (duid_len*3 - 1):0)+1;
//     if(outsz<need) return -1;
//     char *p=out;
//     for(uint16_t i=0;i<duid_len;i++)
//     {
//         int n=snprintf(p,outsz-(size_t)(p-out),(i+1<duid_len)? "%02x:" : "%02x", duid[i]);
//         if(n<0) return -1;
//         p+=n;
//     }
//     *p='\0';
//     return (int)(p-out);
// }  

const char* lease_v6_state_to_string(lease_state_t s)
{
    switch(s)
    {
        case LEASE_STATE_FREE: return "free";
        case LEASE_STATE_ACTIVE: return "active";
        case LEASE_STATE_EXPIRED: return "expired";
        case LEASE_STATE_RELEASED: return "released";
        case LEASE_STATE_ABANDONED: return "abandoned";
        case LEASE_STATE_RESERVED: return "reserved";
        case LEASE_STATE_BACKUP: return "backup";
        default:                    return "unknown";
    }
}

lease_state_t lease_v6_state_from_string(const char* s)
{
    if(!s) return LEASE_STATE_FREE;
    if(strcmp(s,"free")==0) return LEASE_STATE_FREE;
    if(strcmp(s,"active")==0) return LEASE_STATE_ACTIVE;
    if(strcmp(s,"expired")==0) return LEASE_STATE_EXPIRED;
    if(strcmp(s,"released")==0) return LEASE_STATE_RELEASED;
    if(strcmp(s,"abandoned")==0) return LEASE_STATE_ABANDONED;
    if(strcmp(s,"reserved")==0) return LEASE_STATE_RESERVED;
    if(strcmp(s,"backup")==0) return LEASE_STATE_BACKUP;
    return LEASE_STATE_FREE;
}

bool lease_v6_is_expired(const dhcpv6_lease_t* L)
{
    time_t now=time(NULL);
    return (L->state == LEASE_STATE_ACTIVE && L->ends < now);
}

int lease_v6_db_init(lease_v6_db_t* db, const char* path)
{
    if(!db || !path) return -1;
    memset(db, 0, sizeof(*db));
    strncpy(db->filename,path, sizeof(db->filename)-1);
    db->capacity=128;
    log_info("v6-db init file=%s", db->filename);
    return 0;
}

void lease_v6_db_free(lease_v6_db_t* db)
{
    if(!db) return;
    log_info("v6-db free (count=%u)",db->count);
    memset(db,0,sizeof(*db));
}

static int rd_open(rd_ctx_t* R, const char* path)
{
    memset(R,0,sizeof(*R));
    R->fd=open(path,O_RDONLY);
    if(R->fd<0) return -1;
    return 0;
}

static int rd_getline(rd_ctx_t* R, char *out, size_t outsz)
{
    size_t o=0;
    for(;;)
    {
        if(R->pos >= R->len)
        {
            ssize_t n = read(R->fd,R->buf,sizeof(R->buf));
            if(n<0)
            {
                if(errno==EINTR)
                    continue;
                return -1;
            }

            if(n==0)
            {
                if(o>0)
                {
                    out[o]='\0';
                    return 1;
                }
                return 0;
            }
            R->len = (size_t)n;
            R->pos = 0;

        }
        char c = R->buf[R->pos++];
        if(c=='\n')
        {
            out[o]='\0';
            return 1;
        }
        if(o+1 < outsz)
            out[o++]=c;
    }
}

static void rd_close(rd_ctx_t* R)
{
    if(R->fd >= 0)
    {
        close(R->fd);
        R->fd=-1;
    }
}

static int parse_block_ia_na(rd_ctx_t* R, dhcpv6_lease_t* L, char* line0)
{
    memset(L,0,sizeof(*L));
    L->type=Lease6_IA_NA;
    L->state = LEASE_STATE_FREE;

    char* p= trim(line0);
    // Use proper buffer size for IP string
    char ip[LEASE_V6_STR_MAX];
    if(sscanf(p,"lease %95s {",ip)!=1) return -1;
    
    // Validate length before copying to avoid potential issues even if sscanf limited it
    if(str_to_in6(ip, &L->ip6_addr) != 0) return -1;
    
    in6_to_str(&L->ip6_addr,L->ip6_addr_str,sizeof(L->ip6_addr_str));

    int seen_starts = 0, seen_ends = 0;

    char line[READ_BUF_SZ];
    while(1)
    {
        int rc = rd_getline(R,line,sizeof(line));

        if(rc<=0) return -1;
        char* s = trim(line);
        if(!*s || *s == '#') continue;
        if(*s == '}'){
            if (!seen_starts && !seen_ends) return -1;
            return 0;
        }

        if (!strncmp(s,"starts",6)) {
            char val[128];
            if (sscanf(s, "starts %127[^;];", val) == 1) {
                time_t t = parse_lease_time_any(trim(val));
                if (t > 0) { L->starts = t; seen_starts = 1; }
            }
            continue;
        }
        if (!strncmp(s,"ends",4)) {
             char val[128];
             if (sscanf(s, "ends %127[^;];", val) == 1) {
                 time_t t = parse_lease_time_any(trim(val));
                 if (t > 0) { L->ends = t; seen_ends = 1; }
             }
             continue;
        }
        if(!strncmp(s,"duid",4))
        {
            char hex[3*DUID_MAX_LEN];
            if(sscanf(s,"duid %383[^;];",hex)==1)
            {
                trim(hex);
                int n = duid_hex_to_bin(hex,L->duid,DUID_MAX_LEN);
                if(n<0) return -1;
                L->duid_len=(uint16_t)n;
            }
        }
        if(!strncmp(s,"iaid",4))
        {
            unsigned v=0;
            if(sscanf(s,"iaid %u;",&v)==1)
                L->iaid=v;
        }
        if(!strncmp(s,"client-hostname",15))
        {
            char hn[HOSTNAME6_MAX];
            if(sscanf(s,"client-hostname \"%127[^\"]\";",hn)==1)
            {
                strncpy(L->client_hostname,hn,sizeof(L->client_hostname)-1);
            }
        }
        if(!strncmp(s,"binding state",13))
        {
            char st[16];
            if(sscanf(s,"binding state %15[^;];",st)==1)
            {
                trim(st);
                L->state=lease_v6_state_from_string(st);
                strncpy(L->binding_state,st,sizeof(L->binding_state)-1);
            }
        }
    }
}

static int parse_block_ia_pd(rd_ctx_t* R, dhcpv6_lease_t* L, char* line0)
{
    memset(L,0,sizeof(*L));
    L->type =  Lease6_IA_PD;
    L->state=LEASE_STATE_FREE;

    char* p = trim(line0);
    char ip[LEASE_V6_STR_MAX];
    uint8_t plen = 0;
    
    // Expected format: prefix <ip>/<plen> {
    if(sscanf(p, "prefix %95[^/]/%hhu {", ip, &plen)!=2) return -1;

    if(str_to_in6(ip, &L->prefix_v6)!=0) return -1;
    L->plen = plen;

    in6_to_str(&L->prefix_v6, L->prefix_str, sizeof(L->prefix_str));

    int seen_starts = 0, seen_ends = 0;
    char line[READ_BUF_SZ];
    while(1)
    {
        int rc = rd_getline(R,line,sizeof(line));
        if(rc<0) return -1;
        char* s=trim(line);
        if(!*s || *s=='#') continue;
        if(*s == '}'){
            if (!seen_starts && !seen_ends) return -1;
            return 0;
        }

        if (!strncmp(s,"starts",6)) {
            char val[128];
            if (sscanf(s, "starts %127[^;];", val) == 1) {
                time_t t = parse_lease_time_any(trim(val));
                if (t > 0) { L->starts = t; seen_starts = 1; }
            }
            continue;
        }
        if (!strncmp(s,"ends",4)) {
             char val[128];
             if (sscanf(s, "ends %127[^;];", val) == 1) {
                 time_t t = parse_lease_time_any(trim(val));
                 if (t > 0) { L->ends = t; seen_ends = 1; }
             }
             continue;
        }
        if(!strncmp(s,"duid",4))
        {
            char hex[3*DUID_MAX_LEN];
            if(sscanf(s,"duid %383[^;];",hex)==1)
            {
                trim(hex);
                int n = duid_hex_to_bin(hex,L->duid,DUID_MAX_LEN);
                if(n<0)
                    return -1;
                L->duid_len=(uint16_t)n;
            }
        }
        if(!strncmp(s,"iaid",4))
        {
            unsigned v= 0;
            if(sscanf(s,"iaid %u;",&v)==1)
                L->iaid=v;
        }
        if(!strncmp(s,"client-hostname",15))
        {
            char hn[HOSTNAME6_MAX];
            if(sscanf(s,"client-hostname \"%127[^\"]\";",hn)==1)
                strncpy(L->client_hostname,hn,sizeof(L->client_hostname)-1);
        }
        if(!strncmp(s,"binding state",13))
        {
            char st[16];
            if(sscanf(s,"binding state %15[^;];",st)==1)
            {
                trim(st);
                L->state=lease_v6_state_from_string(st);
                strncpy(L->binding_state, st, sizeof(L->binding_state)-1);
            }
        }
    }
}

int lease_v6_db_load(lease_v6_db_t* db)
{
    if(!db) return -1;
    rd_ctx_t R;
    if(rd_open(&R,db->filename)<0)
    {
        log_warn("v6-db: %s not found, starting empty",db->filename);
        return 0;
    }
    db->count=0;

    char line[READ_BUF_SZ];
    while(1)
    {
        int rc=rd_getline(&R,line,sizeof(line));
        if(rc<=0) break;
        char* s = trim(line);
        if(!*s || *s=='#') continue;

        if(!strncmp(s,"lease ",6))
        {
            dhcpv6_lease_t tmp;
            if (parse_block_ia_na(&R, &tmp, s) == 0) {
                 if (tmp.starts && tmp.ends) {
                     // Check for existing
                     int found = -1;
                     for(uint32_t i=0; i<db->count; i++) {
                         if (db->leases[i].type == Lease6_IA_NA && 
                             memcmp(&db->leases[i].ip6_addr, &tmp.ip6_addr, sizeof(tmp.ip6_addr))==0) {
                             found = i;
                             break;
                         }
                     }
                     if (found >= 0) {
                         // Overwrite existing (newer entry in log)
                         db->leases[found] = tmp;
                         db->leases[found].in_use = 1;
                     } else {
                         if (db->count < LEASES6_MAX) {
                             db->leases[db->count++] = tmp; 
                             db->leases[db->count-1].in_use=1; 
                         } else {
                             log_warn("v6-db: DB full, dropping lease %s", s);
                         }
                     }
                 }
                 else log_warn("v6-db: dropping NA w/o time");
            }
            else log_warn("v6-db: bad IA-NA block, skipping");
        }
        else if(!strncmp(s,"prefix ",7))
        {
            dhcpv6_lease_t tmp;
            if (parse_block_ia_pd(&R, &tmp, s) == 0) {
                 if (tmp.starts && tmp.ends) {
                     // Check for existing
                     int found = -1;
                     for(uint32_t i=0; i<db->count; i++) {
                         if (db->leases[i].type == Lease6_IA_PD && 
                             db->leases[i].plen == tmp.plen &&
                             memcmp(&db->leases[i].prefix_v6, &tmp.prefix_v6, sizeof(tmp.prefix_v6))==0) {
                             found = i;
                             break;
                         }
                     }
                     if (found >= 0) {
                         // Overwrite
                         db->leases[found] = tmp;
                         db->leases[found].in_use = 1;
                     } else {
                         if (db->count < LEASES6_MAX) {
                             db->leases[db->count++] = tmp; 
                             db->leases[db->count-1].in_use=1; 
                         } else {
                             log_warn("v6-db: DB full, dropping prefix %s", s);
                         }
                     }
                 }
                 else log_warn("v6-db: dropping PD w/o time");
            }
            else log_warn("v6-db: bad IA_PD block, skipping");
        }
    }
    rd_close(&R);
    log_info("v6-db loaded %u unique entries from %s", db->count, db->filename);
    return 0;
}

int lease_v6_db_save(lease_v6_db_t* db)
{
    if(!db) return -1;

    char tmp_path[LEASE6_PATH_MAX + 8];
    snprintf(tmp_path,sizeof(tmp_path),"%s.tmp",db->filename);

    int fd=open(tmp_path,O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd<0)
    {
        log_error("v6-db: open(%s) failed:%s",tmp_path, strerror(errno));
        return -1;
    }

    time_t now=time(NULL);

    char tbuf[64];
    format_lease_time(now, tbuf, sizeof(tbuf));

    write_fmt(fd, "# The format of this file is documented in the dhcpd.leases(5) manual page.\n");
    write_fmt(fd, "# This lease file was written by DHCPv6 Server\n#\n");
    write_fmt(fd, "authoring-byte-order little-endian;\n\n");
    write_fmt(fd, "# Server DUID (hex, informational)\n");
    if(write_fmt(fd,"# THis file is automatically generated, do not edit manually\n")<0) goto fail;
    write_fmt(fd, "# Lease Database Format (DHCPv6)\n");

    write_fmt(fd, "# lease <ipv6-address> {\n");
    write_fmt(fd, "#   duid <hex>; iaid <n>;\n");
    write_fmt(fd, "#   starts <w YYYY/MM/DD HH:MM:SS>; ends <...>;\n");
    write_fmt(fd, "#   tstp <...>; cltt <...>;\n");
    write_fmt(fd, "#   binding state <state>; next binding state <state>; rewind binding state <state>;\n");
    write_fmt(fd, "#   client-hostname \"...\"; vendor-class \"...\"; fqdn \"...\";\n");
    write_fmt(fd, "# }\n");
    write_fmt(fd, "# prefix <ipv6>/<plen> { ... }  # for IA_PD\n");
    if (write_fmt(fd, "# Last updated: %s\n", ctime(&now)) < 0) goto fail;
    char duid_hex[3*DUID_MAX_LEN];
    for(uint32_t i=0;i<db->count;i++)
    {
        dhcpv6_lease_t* L = &db->leases[i];
        if (!L->in_use) continue;
        char tb[64];

       
        duid_hex[0]='\0';
        if(L->duid_len)
        {
            if(duid_bin_to_hex(L->duid,L->duid_len,duid_hex,sizeof(duid_hex))<0)
                duid_hex[0]='\0';
        }

        if (L->type == Lease6_IA_NA){
            char ip[LEASE_V6_STR_MAX]; 
            in6_to_str(&L->ip6_addr, ip, sizeof(ip));
            if (write_fmt(fd, "\nlease %s {\n", ip) < 0) goto fail;
            format_lease_time(L->starts, tb, sizeof(tb));
            if (write_fmt(fd, "\tstarts %s;\n", tb) < 0) goto fail;
            format_lease_time(L->ends, tb, sizeof(tb));
            if (write_fmt(fd, "\tends %s;\n", tb)   < 0) goto fail;
            if (L->tstp) { format_lease_time(L->tstp, tb, sizeof(tb)); write_fmt(fd, "\ttstp %s;\n", tb); }
            if (L->cltt) { format_lease_time(L->cltt, tb, sizeof(tb)); write_fmt(fd, "\tcltt %s;\n", tb); }
            if (L->duid_len && write_fmt(fd, "\tduid %s;\n", duid_hex) < 0) goto fail;
            if (write_fmt(fd, "\tiaid %u;\n", L->iaid) < 0) goto fail;
            if (write_fmt(fd, "\tbinding state %s;\n", lease_v6_state_to_string(L->state)) < 0) goto fail;
            if (L->next_state)   write_fmt(fd, "\tnext binding state %s;\n",   lease_v6_state_to_string(L->next_state));
            if (L->rewind_state) write_fmt(fd, "\trewind binding state %s;\n", lease_v6_state_to_string(L->rewind_state));
            if (L->client_hostname[0] && write_fmt(fd, "\tclient-hostname \"%s\";\n", L->client_hostname) < 0) goto fail;
            if (L->vendor_class[0]) write_fmt(fd, "\tvendor-class \"%s\";\n", L->vendor_class);
            if (L->fqdn[0])         write_fmt(fd, "\tfqdn \"%s\";\n", L->fqdn);
            if (write_fmt(fd, "}\n") < 0) goto fail;
        }
        else {
            char pfx[LEASE_V6_STR_MAX]; in6_to_str(&L->prefix_v6, pfx, sizeof(pfx));
            if (write_fmt(fd, "\nprefix %s/%u {\n", pfx, L->plen) < 0) goto fail;
            format_lease_time(L->starts, tb, sizeof(tb));
            if (write_fmt(fd, "\tstarts %s;\n", tb) < 0) goto fail;
            format_lease_time(L->ends, tb, sizeof(tb));
            if (write_fmt(fd, "\tends %s;\n", tb)   < 0) goto fail;
            if (L->tstp) { format_lease_time(L->tstp, tb, sizeof(tb)); write_fmt(fd, "\ttstp %s;\n", tb); }
            if (L->cltt) { format_lease_time(L->cltt, tb, sizeof(tb)); write_fmt(fd, "\tcltt %s;\n", tb); }
            if (L->duid_len && write_fmt(fd, "\tduid %s;\n", duid_hex) < 0) goto fail;
            if (write_fmt(fd, "\tiaid %u;\n", L->iaid) < 0) goto fail;
            if (write_fmt(fd, "\tbinding state %s;\n", lease_v6_state_to_string(L->state)) < 0) goto fail;
            if (L->next_state)   write_fmt(fd, "\tnext binding state %s;\n",   lease_v6_state_to_string(L->next_state));
            if (L->rewind_state) write_fmt(fd, "\trewind binding state %s;\n", lease_v6_state_to_string(L->rewind_state));
            if (L->client_hostname[0] && write_fmt(fd, "\tclient-hostname \"%s\";\n", L->client_hostname) < 0) goto fail;
            if (L->vendor_class[0]) write_fmt(fd, "\tvendor-class \"%s\";\n", L->vendor_class);
            if (L->fqdn[0])  write_fmt(fd, "\tfqdn \"%s\";\n", L->fqdn);
            if (write_fmt(fd, "}\n") < 0) goto fail;
        }
    }

    if (fsync(fd) < 0){ log_warn("v6-db: fsync file failed: %s", strerror(errno)); }
    if(close(fd)<0) return -1;
    

    if (rename(tmp_path, db->filename) < 0){
        log_error("v6-db: rename(%s->%s) failed: %s", tmp_path, db->filename, strerror(errno));
        return -1;
    }
    (void)fsync_dirname(db->filename);
    log_info("v6-db saved %u entries to %s", db->count, db->filename);
    return 0;

fail:
    { int e=errno; close(fd); unlink(tmp_path); errno=e; }
    log_error("v6-db: save failed: %s", strerror(errno));
    return -1;
}


int lease_v6_db_append(lease_v6_db_t* db, const dhcpv6_lease_t* L){
    if (!db || !L) return -1;

    int fd = open(db->filename, O_WRONLY|O_CREAT|O_APPEND, 0644);
    if (fd < 0){ log_error("v6-db: open(%s) append failed: %s", db->filename, strerror(errno)); return -1; }
    char addr[IP6_STR_MAX], tbuf[64];
    char duid_hex[3*DUID_MAX_LEN]; duid_hex[0]='\0';
    if (L->duid_len){
        if (duid_bin_to_hex(L->duid, L->duid_len, duid_hex, sizeof(duid_hex)) < 0) duid_hex[0]='\0';
    }

    if (L->type == Lease6_IA_NA) {
        inet_ntop(AF_INET6, &L->ip6_addr, addr, sizeof(addr));
        write_fmt(fd, "lease %s {\n", addr);
    } else {
         inet_ntop(AF_INET6, &L->prefix_v6, addr, sizeof(addr));
         write_fmt(fd, "prefix %s/%u {\n", addr, (unsigned)L->plen);
     }
     write_fmt(fd, "\tduid %s;\n", duid_hex);
     write_fmt(fd, "\tiaid %u;\n", (unsigned)L->iaid);
     format_lease_time(L->starts, tbuf, sizeof(tbuf));
     write_fmt(fd, "\tstarts %s;\n", tbuf);
     format_lease_time(L->ends, tbuf, sizeof(tbuf));
     write_fmt(fd, "\tends %s;\n", tbuf);
     if (L->tstp) { format_lease_time(L->tstp, tbuf, sizeof(tbuf)); write_fmt(fd, "\ttstp %s;\n", tbuf); }
     if (L->cltt) { format_lease_time(L->cltt, tbuf, sizeof(tbuf)); write_fmt(fd, "\tcltt %s;\n", tbuf); }
 
     write_fmt(fd, "\tbinding state %s;\n", lease_v6_state_to_string(L->state));
     if (L->next_state)   write_fmt(fd, "\tnext binding state %s;\n",   lease_v6_state_to_string(L->next_state));
     if (L->rewind_state) write_fmt(fd, "\trewind binding state %s;\n", lease_v6_state_to_string(L->rewind_state));
 
     if (L->client_hostname[0])     write_fmt(fd, "\tclient-hostname \"%s\";\n", L->client_hostname);
     if (L->vendor_class[0]) write_fmt(fd, "\tvendor-class \"%s\";\n", L->vendor_class);
     if (L->fqdn[0])   write_fmt(fd, "\tfqdn \"%s\";\n", L->fqdn);
     write_fmt(fd, "}\n\n");
    if (fsync(fd) < 0)
    { 
        log_warn("v6-db: fsync append file failed: %s", strerror(errno)); 
    }
    close(fd);
  
    log_info("v6-db append one (%s)", (L->type==Lease6_IA_NA)?"IA_NA":"IA_PD");
    return 0;
}

dhcpv6_lease_t* lease_v6_add_ia_na(lease_v6_db_t* db,
                                   const char* duid_hex, uint16_t duid_len, uint32_t iaid,
                                   const struct in6_addr* ip,
                                   uint32_t lease_secs,
                                   const char* hostname_opt)
{
   (void)duid_len;
   if (!db || !duid_hex || !ip) return NULL;

   // Search for existing lease for this IP
   dhcpv6_lease_t* L = NULL;
   for(uint32_t i=0; i<db->count; i++) {
       if (db->leases[i].in_use && 
           db->leases[i].type == Lease6_IA_NA &&
           memcmp(&db->leases[i].ip6_addr, ip, sizeof(*ip)) == 0) {
           L = &db->leases[i];
           break;
       }
   }

   if (!L) {
       if (db->count >= LEASES6_MAX) {
            log_error("v6 add IA_NA: lease DB full");
            return NULL;
        }
        L = &db->leases[db->count];
        memset(L,0,sizeof(*L));
        db->count++;
   }

    L->in_use=1; 
    L->type=Lease6_IA_NA;
    if (duid_hex && *duid_hex){
        int n = duid_hex_to_bin(duid_hex, L->duid, DUID_MAX_LEN);
        if (n < 0) { log_error("v6 add IA_NA: invalid DUID hex"); return NULL; }
        L->duid_len = (uint16_t)n;
    }
    time_t now=time(NULL);
    L->iaid = iaid;
    L->ip6_addr = *ip; 
    in6_to_str(ip, L->ip6_addr_str, sizeof(L->ip6_addr_str));
    L->starts = time(NULL);
    L->ends   = L->starts + lease_secs;
    L->tstp   = now;
    L->cltt   = now;
    L->state  = LEASE_STATE_ACTIVE;
    if (hostname_opt) strncpy(L->client_hostname, hostname_opt, sizeof(L->client_hostname)-1);
    
    // Always append to disk log
    (void)lease_v6_db_append(db, L);
    
    char duid_dbg[3*DUID_MAX_LEN]; duid_dbg[0]='\0';
    (void)duid_bin_to_hex(L->duid, L->duid_len, duid_dbg, sizeof(duid_dbg));
    log_info("v6 add IA_NA duid=%s iaid=%u ip=%s lease=%us", duid_dbg, L->iaid, L->ip6_addr_str, (unsigned)lease_secs);
    return L;
}


dhcpv6_lease_t* lease_v6_add_ia_pd(lease_v6_db_t* db,
                                   const char* duid_hex, uint16_t duid_len, uint32_t iaid,
                                   const struct in6_addr* prefix_base,
                                   uint8_t plen,
                                   uint32_t lease_secs,
                                   const char* hostname_opt)
{
   (void) duid_len;
    if (!db || !prefix_base) return NULL;

   // Search for existing lease for this Prefix
   dhcpv6_lease_t* L = NULL;
   for(uint32_t i=0; i<db->count; i++) {
       if (db->leases[i].in_use && 
           db->leases[i].type == Lease6_IA_PD &&
           db->leases[i].plen == plen &&
           memcmp(&db->leases[i].prefix_v6, prefix_base, sizeof(*prefix_base)) == 0) {
           L = &db->leases[i];
           break;
       }
   }

    if (!L) {
        if (db->count >= LEASES6_MAX) {
            log_error("v6 add IA_PD: lease DB full");
            return NULL;
        }
        L = &db->leases[db->count];
        memset(L,0,sizeof(*L));
        db->count++;
    }

    L->in_use=1; L->type=Lease6_IA_PD;
    if (duid_hex && *duid_hex){
        int n = duid_hex_to_bin(duid_hex, L->duid, DUID_MAX_LEN);
        if (n < 0) { log_error("v6 add IA_PD: invalid DUID hex"); return NULL; }
        L->duid_len = (uint16_t)n;
    }
    L->iaid = iaid;
    L->prefix_v6 = *prefix_base; in6_to_str(prefix_base, L->prefix_str, sizeof(L->prefix_str));
    L->plen = plen;
    time_t now=time(NULL);
    L->starts = time(NULL);
    L->ends   = L->starts + lease_secs;
    L->tstp   = now;
    L->cltt   = now;
    L->state  = LEASE_STATE_ACTIVE;
    if (hostname_opt) strncpy(L->client_hostname, hostname_opt, sizeof(L->client_hostname)-1);

    // Always append to disk log
    (void)lease_v6_db_append(db, L);

    char duid_dbg[3*DUID_MAX_LEN]; duid_dbg[0]='\0';
    (void)duid_bin_to_hex(L->duid, L->duid_len, duid_dbg, sizeof(duid_dbg));
    log_info("v6 add IA_PD duid=%s iaid=%u prefix=%s/%u lease=%us", duid_dbg, L->iaid, L->prefix_str, L->plen, (unsigned)lease_secs);
    return L;
}


dhcpv6_lease_t* lease_v6_find_by_ip(lease_v6_db_t* db, const struct in6_addr* ip){
    if (!db || !ip) return NULL;
    for (uint32_t i=0;i<db->count;i++){
        dhcpv6_lease_t* L=&db->leases[i];
        if (!L->in_use || L->type!=Lease6_IA_NA) continue;
        if (memcmp(&L->ip6_addr, ip, sizeof(struct in6_addr))==0) return L;
    }
    return NULL;
}


dhcpv6_lease_t* lease_v6_find_by_prefix(lease_v6_db_t* db, const struct in6_addr* pfx, uint8_t plen){
    if (!db || !pfx) return NULL;
    for (uint32_t i=0;i<db->count;i++){
        dhcpv6_lease_t* L=&db->leases[i];
        if (!L->in_use || L->type!=Lease6_IA_PD) continue;
        if (L->plen!=plen) continue;
        if (memcmp(&L->prefix_v6, pfx, sizeof(struct in6_addr))==0) return L;
    }
    return NULL;
}

dhcpv6_lease_t* lease_v6_find_by_duid_iaid(lease_v6_db_t* db, const uint8_t* duid, uint16_t duid_len, uint32_t iaid, lease_v6_type_t type){
    if (!db || !duid) return NULL;
    for (uint32_t i=0;i<db->count;i++){
        dhcpv6_lease_t* L=&db->leases[i];
        if (!L->in_use || L->type!=type) continue;
        if (L->iaid!=iaid) continue;
        if (L->duid_len==duid_len && memcmp(L->duid, duid, duid_len)==0) return L;
    }
    return NULL;
}

int lease_v6_release_ip(lease_v6_db_t* db, const struct in6_addr* ip){
    if (!db || !ip) return -1;
    dhcpv6_lease_t* L = lease_v6_find_by_ip(db, ip);
    if (!L) return -1;
    L->state = LEASE_STATE_RELEASED;
    L->ends  = time(NULL);
    log_info("v6 release IA_NA ip=%s", L->ip6_addr_str);
    return lease_v6_db_save(db);
}

int lease_v6_release_prefix(lease_v6_db_t* db, const struct in6_addr* pfx, uint8_t plen){
    if (!db || !pfx) return -1;
    dhcpv6_lease_t* L = lease_v6_find_by_prefix(db, pfx, plen);
    if (!L) return -1;
    L->state = LEASE_STATE_RELEASED;
    L->ends  = time(NULL);
    log_info("v6 release IA_PD %s/%u", L->prefix_str, L->plen);
    return lease_v6_db_save(db);
}

int lease_v6_renew_ip(lease_v6_db_t* db, const struct in6_addr* ip, uint32_t lease_secs){
    if (!db || !ip) return -1;
    dhcpv6_lease_t* L = lease_v6_find_by_ip(db, ip);
    if (!L) return -1;
    time_t now=time(NULL);
    L->starts = now; L->ends = now + lease_secs; L->state=LEASE_STATE_ACTIVE;
    log_info("v6 renew IA_NA ip=%s lease=%us", L->ip6_addr_str, (unsigned)lease_secs);
    return lease_v6_db_save(db);
}

int lease_v6_renew_prefix(lease_v6_db_t* db, const struct in6_addr* pfx, uint8_t plen, uint32_t lease_secs){
    if (!db || !pfx) return -1;
    dhcpv6_lease_t* L = lease_v6_find_by_prefix(db, pfx, plen);
    if (!L) return -1;
    time_t now=time(NULL);
    L->starts = now; L->ends = now + lease_secs; L->state=LEASE_STATE_ACTIVE;
    log_info("v6 renew IA_PD %s/%u lease=%us", L->prefix_str, L->plen, (unsigned)lease_secs);
    return lease_v6_db_save(db);
}


int lease_v6_mark_expired_older(lease_v6_db_t* db){
    if (!db) return -1;
    time_t now=time(NULL); uint32_t n=0;
    for (uint32_t i=0;i<db->count;i++){
        dhcpv6_lease_t* L=&db->leases[i];
        if (!L->in_use) continue;
        if (L->state==LEASE_STATE_ACTIVE && L->ends < now){ L->state=LEASE_STATE_EXPIRED; n++; }
    }
    if (n) (void)lease_v6_db_save(db);
    log_info("v6 mark-expired: %u", n);
    return (int)n;
}

int lease_v6_cleanup(lease_v6_db_t* db){
    if (!db) return -1;
    uint32_t removed=0, i=0;
    while(i<db->count){
        dhcpv6_lease_t* L=&db->leases[i];
        if (L->in_use && (L->state==LEASE_STATE_EXPIRED || L->state==LEASE_STATE_RELEASED)){
            memmove(&db->leases[i], &db->leases[i+1], (db->count-i-1)*sizeof(dhcpv6_lease_t));
            db->count--; removed++;
        } else i++;
    }
    if (removed) (void)lease_v6_db_save(db);
    log_info("v6 cleanup removed=%u", removed);
    return (int)removed;
}

void lease_v6_db_print(const lease_v6_db_t* db){
    if (!db) return;
    printf("--- DHCPv6 Lease DB ---\nFile: %s\nTotal: %u\n\n", db->filename, db->count);
     for (uint32_t i=0;i<db->count;i++){
         const dhcpv6_lease_t *L = &db->leases[i];
         char a[INET6_ADDRSTRLEN], sb[64], eb[64];
         if (L->type == Lease6_IA_NA) {
             inet_ntop(AF_INET6, &L->ip6_addr, a, sizeof(a));
         } else {
             inet_ntop(AF_INET6, &L->prefix_v6, a, sizeof(a));
         }
         format_lease_time(L->starts, sb, sizeof(sb));
         format_lease_time(L->ends,   eb, sizeof(eb));
         dprintf(STDOUT_FILENO, "[%u] %s %s  iaid=%u  starts=%s  ends=%s  state=%d\n",
                 i, (L->type==Lease6_IA_NA?"IA_NA":"IA_PD"),
                 a, (unsigned)L->iaid, sb, eb, (int)L->state);
    }
}

int lease_v6_mark_reserved(lease_v6_db_t *db,
                           const struct in6_addr *ip6,
                           const char *duid_hex,
                           uint32_t iaid,
                           const char *hostname)
{
    if (!db || !ip6 || !duid_hex) return -1;

    dhcpv6_lease_t *L = lease_v6_find_by_ip(db, ip6);

    if(!L) {
        if (db->count >= LEASES6_MAX) return -1;
        L = &db->leases[db->count++];
        memset(L, 0, sizeof(*L));
        L->in_use   = 1;
        L->type     = Lease6_IA_NA;
        L->ip6_addr = *ip6;
    }

    int n = duid_hex_to_bin(duid_hex, L->duid, DUID_MAX_LEN);
    if (n < 0) return -1;
    L->duid_len = (uint16_t)n;
    L->iaid = iaid;

    if (hostname)
    {
        strncpy(L->client_hostname, hostname, sizeof(L->client_hostname) - 1);
        L->client_hostname[sizeof(L->client_hostname) - 1] = '\0';
    }

    time_t now = time(NULL);
    L->starts = now;
    L->ends = now + 86400;  
    L->tstp = now;
    L->cltt = now;

    L->state = LEASE_STATE_RESERVED;
    L->next_state = LEASE_STATE_FREE;
    L->rewind_state = LEASE_STATE_FREE;

    return lease_v6_db_save(db);
}

int lease_v6_set_state(lease_v6_db_t* db, const struct in6_addr* ip6_addr, lease_state_t new_state)
{
     if (!db || !ip6_addr) return -1;
    dhcpv6_lease_t* L = lease_v6_find_by_ip(db, ip6_addr);

    if (!L) {
        if (new_state == LEASE_STATE_ACTIVE || new_state == LEASE_STATE_RESERVED) {
            if (db->count >= LEASES6_MAX) return -1;
            L = &db->leases[db->count++];
            memset(L, 0, sizeof(*L));
            L->in_use   = 1;
            L->type     = Lease6_IA_NA;
            L->ip6_addr = *ip6_addr;
        } else {
            return 0;
        }
    }

    L->state = new_state;

    time_t now = time(NULL);
    if (!L->starts) L->starts = now;

    if (new_state == LEASE_STATE_ACTIVE) {
        if (!L->ends || L->ends < now) L->ends = now + 3600;
    } else if (new_state == LEASE_STATE_RESERVED) {
        if (!L->ends || L->ends < now) L->ends = now + 86400;
    } else {
        L->ends = now;
}


    return lease_v6_db_save(db);
}

int lease_v6_mark_conflict(lease_v6_db_t* db, const struct in6_addr* ip6_addr, const char* reason)
{
    if (!db || !ip6_addr) return -1;
    char ipstr[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, ip6_addr, ipstr, sizeof(ipstr));
    log_warn("v6 conflict on %s (%s)", ipstr, reason ? reason : "probe");
    return lease_v6_set_state(db, ip6_addr, LEASE_STATE_ABANDONED);
}

