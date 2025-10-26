#include "config_v6.h"
#include "utilsv6.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <arpa/inet.h>


#define LINE_MAX_LEN 512

//functie care taie \n si \r 
static void rstrip(char *str)
{
    int len = (int)strlen(str);
    while(len > 0 &&(str[len-1]=='\n' || str[len-1]=='\r'))
    {
        str[len-1]='\0';
        len--;
    }
}

//skip spatii/tabs
static char *lskip(char *s)
{
    while(*s &&(*s ==' ' || *s=='\t'))
    {
        s++;
    }
    return s;
}

static void strip_semicolon(char *s)
{
    rstrip(s);
    int len = (int) strlen(s);
    while(len > 0 && (s[len-1]==';' || s[len-1]==' ' || s[len-1]=='\t'))
    {
        s[len-1]='\0';
        len--;
    }
}

static void unquote(char *s)
{
    size_t len=strlen(s);
    if(len>=2 && s[0]=='"' && s[len-1]=='"')
    {
        memmove(s,s+1,len-2);
        s[len-2]='\0';
    }
}

static int parse_prefix_and_len(const char *in, char *prefix_out,size_t prefix_out_sz,uint8_t *len_out)
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

//verifica daca linia incepe cu un anumit cuvant cheie
static bool starts_with(const char *line,const char *kw)
{
    while(*kw && *line == *kw)
    {
        line++;
        kw++;
    }
    return(*kw =='\0');
}

static void parse_global_option(dhcpv6_config_t *cfg, char *line)
{
    strip_semicolon(line);
    line=lskip(line);

    if(starts_with(line,"default-lease-time"))
    {
        unsigned val = 0;
        sscanf(line,"default-lease-time %u",&val);
        cfg->global.default_lease_time=(uint32_t)val;
        return;
    }

    if(starts_with(line,"max-lease-time"))
    {
        unsigned val = 0;
        sscanf(line,"max-lease-time %u",&val);
        cfg->global.max_lease_time=(uint32_t)val;
        return;
    }

    if(starts_with(line,"option dhcp6.name-servers"))
    {
        const char *p=strstr(line,"option dhcp6.name-servers");
        if(p)
        {
            p+=strlen("option dhcp6.name-servers");
            p=lskip((char*)p);
            strncpy(cfg->global.global_dns_servers,p,sizeof(cfg->global.global_dns_servers)-1);
        }
        return;
    }

    if(starts_with(line,"option dhcp6.domain-search"))
    {
        const char *p = strstr(line,"option dhcp6.domain-search");
        if(p)
        {
            p+=strlen("option dhcp6.domain-search");
            p=lskip((char*)p);

            char tmp[256];
            strncpy(tmp,p,sizeof(tmp)-1);
            tmp[sizeof(tmp)-1]='\0';
            unquote(tmp);

            strncpy(cfg->global.global_domain_search, tmp, sizeof(cfg->global.global_domain_search)-1);
        }
        return;
    }
}

static int parse_host_block(FILE *fp, dhcpv6_subnet_t *subnet, const char *first_line)
{
    char hostname[HOSTNAME_MAX]={0};
    {
        char tmp[HOSTNAME_MAX];
        strncpy(tmp,first_line,sizeof(tmp)-1);
        tmp[sizeof(tmp)-1]='\0';

        char *brace = strchr(tmp,'{');
        if(brace) *brace='\0';

        char kw[HOSTNAME_MAX];
        sscanf(tmp,"host %255s", hostname);
        (void)kw;
    }

    if(subnet->host_count >= MAX_HOSTS_PER_SUBNET)
    {
        return -1;
    }

    dhcpv6_static_host_t *h = &subnet->hosts[subnet->host_count];
    memset(h,0,sizeof(*h));
    strncpy(h->hostname,hostname,sizeof(h->hostname)-1);

    char line[LINE_MAX_LEN];
    while(fgets(line,sizeof(line),fp))
    {
        rstrip(line);
        char *p=lskip(line);

        if(*p == '#' || *p == '\0') continue;

        if(*p == '}')
        {
            subnet->host_count++;
            return 0;
        }

        if(starts_with(p,"host-identifier option dhcp6.client-id"))
        {
            char duid_tmp[DUID_MAX];
            const char *q=strstr(p,"dhcp6.client-id");
            if(q)
            {
                q+=strlen("dhcp6.client-id");
                q=lskip((char*)q);
                strncpy(duid_tmp,q,sizeof(duid_tmp)-1);
                duid_tmp[sizeof(duid_tmp)-1]='\0';
                strip_semicolon(duid_tmp);
                strncpy(h->duid, duid_tmp,sizeof(h->duid)-1);
            }
            continue;
        }

        if(starts_with(p,"fixed-address6"))
        {
            char addr_tmp[Ip6_STR_MAX];
            memset(addr_tmp,0,sizeof(addr_tmp));

            if(sscanf(p,"fixed-address6 %255s",addr_tmp)==1)
            {
                strip_semicolon(addr_tmp);
                strncpy(h->fixed_address6,addr_tmp,sizeof(h->fixed_address6)-1);

               if(ipv6_str_to_bin(addr_tmp,&h->fixed_addr6_bin)==0)
               {
                   h->has_fixed_address6_bin=true;
               }
               else
               {
                h->has_fixed_address6_bin=false;
               }
            }
            continue;
            
        }

        if(starts_with(p,"option dhcp6.hostname"))
        {
            char host_tmp[HOSTNAME_MAX];
            memset(host_tmp,0,sizeof(host_tmp));

            const char *q = strstr(p,"option dhcp6.hostname");
            if(q)
            {
                q+=strlen("option dhcp6.hostname");
                q=lskip((char*)q);
                strncpy(host_tmp,q,sizeof(host_tmp)-1);
                host_tmp[sizeof(host_tmp)-1]='\0';
                strip_semicolon(host_tmp);
                unquote(host_tmp);
                strncpy(h->hostname,host_tmp,sizeof(h->hostname)-1);
            }
            continue;
        }

        return -1;
    }
}


static void parse_range(dhcpv6_subnet_t *subnet, char *line)
{
    strip_semicolon(line);
    char a[Ip6_STR_MAX], b[Ip6_STR_MAX];

    if(sscanf(line, "range6 %79s %79s",a,b)==2)
    {
        strncpy(subnet->pool_start,a,sizeof(subnet->pool_start)-1);
        strncpy(subnet->pool_end,b,sizeof(subnet->pool_end)-1);
        subnet->pool_start[sizeof(subnet->pool_start)-1]='\0';
        subnet->pool_end[sizeof(subnet->pool_end)-1]='\0';

        if(ipv6_str_to_bin(a,&subnet->pool_start_bin)== 0 && ipv6_str_to_bin(b,&subnet->pool_end_bin)==0)
        {
            subnet->has_pool_range=true;
        }
        else
        {
            subnet->has_pool_range=false;
        }
    }
}

static void parse_prefix6(dhcpv6_subnet_t *subnet, char *line)
{
    strip_semicolon(line);

    char a[Ip6_STR_MAX], b[Ip6_STR_MAX], slashlen[256];

    if(sscanf(line, "prefix6 %79s %79s %255s",a,b,slashlen)==3)
    {
        strip_semicolon(slashlen);
        if(slashlen[0]=='/')
        {
            unsigned plen = (unsigned)strtoul(slashlen+1,NULL,10);
            subnet->pd_prefix_len=(uint8_t)plen;
        }

        strncpy(subnet->pd_pool_start,a,sizeof(subnet->pd_pool_start)-1);
        strncpy(subnet->pd_pool_end,b,sizeof(subnet->pd_pool_end)-1);
        subnet->pd_pool_start[sizeof(subnet->pd_pool_start)-1] = '\0';
        subnet->pd_pool_end[sizeof(subnet->pd_pool_end)-1]='\0';

        if(ipv6_str_to_bin(a,&subnet->pd_pool_start_bin)==0 && ipv6_str_to_bin(b,&subnet->pd_pool_end_bin)==0)
        {
            subnet->pd_enabled=true;
            subnet->has_pd_pool=true;
        }
        else
        {
            subnet->pd_enabled=false;
            subnet->has_pd_pool=false;
        }

    }
}

static void parse_subnet_option(dhcpv6_subnet_t *subnet, char *line)
{
    strip_semicolon(line);
    line=lskip(line);


    if(starts_with(line,"option dhcp6.name-servers"))
    {
        const char *p=strstr(line, "option dhcp6.name-servers");
        if(p)
        {
            p+=strlen("option dhcp6.name-servers");
            p=lskip((char*)p);
            strncpy(subnet->dns_servers,p,sizeof(subnet->dns_servers)-1);
           
        }
        return;
    }

    if(starts_with(line,"option dhcp6.domain-search"))
    {
        const char *p=strstr(line,"option dhcp6.domain-search");
        if(p)
        {
            char tmp[256];
            p+=strlen("option dhcp6.domain-search");
            p=lskip((char*)p);
            strncpy(tmp,p,sizeof(tmp)-1);
            tmp[sizeof(tmp)-1]='\0';
            strip_semicolon(tmp);
            unquote(tmp);
            strncpy(subnet->domain_search, tmp, sizeof(subnet->domain_search)-1);
        }
        return;
    }
}

static dhcpv6_subnet_t *start_new_subnet(dhcpv6_config_t *cfg, char *line)
{
    if(cfg->subnet_count >= MAX_SUBNET_V6)
    {
        return NULL;
    }

    dhcpv6_subnet_t *subnet = &cfg->subnets[cfg->subnet_count];
    memset(subnet,0, sizeof(*subnet));

    char tmp[LINE_MAX_LEN];
    strncpy(tmp,line,sizeof(tmp)-1);
    tmp[sizeof(tmp)-1]='\0';

    char *brace = strchr(tmp,'{');
    if(brace) *brace='\0';
    rstrip(tmp);

    char kw[256],prefix_and_len[256];
    memset(kw,0,sizeof(kw));
    memset(prefix_and_len,0,sizeof(prefix_and_len));

    if(sscanf(tmp,"subnet6 %255s",prefix_and_len)!=1) return NULL;

    if(parse_prefix_and_len(prefix_and_len,subnet->prefix,sizeof(subnet->prefix),&subnet->prefix_len)!=0)
    {
        return NULL;
    }

    if(ipv6_str_to_bin(subnet->prefix,&subnet->prefix_bin)==0)
    {
        subnet->has_prefix_bin=true;
    }

    cfg->subnet_count++;
    return subnet;

}


static void convert_host_block(dhcpv6_static_host_t *h)
{
    if(h->fixed_address6[0]!='\0')
    {
        if(ipv6_str_to_bin(h->fixed_address6,&h->fixed_addr6_bin)==0)
        {
            h->has_fixed_address6_bin=true;
        }
        else
        {
            h->has_fixed_address6_bin=false;
        }
    }  
}

static void convert_range_block(dhcpv6_subnet_t *s)
{
    if(s->pool_start[0]!='\0' && s->pool_end[0]!='\0')
    {

        if(ipv6_str_to_bin(s->pool_start,&s->pool_start_bin)==0 &&
           ipv6_str_to_bin(s->pool_end,&s->pool_end_bin)==0)
        {
            s->has_pool_range=true;
        }
        else
        {
            s->has_pool_range=false;
        }
    }
    else
    {
        s->has_pool_range=false;
    }
}

static void convert_subnet_prefix(dhcpv6_subnet_t *s)
{
    if(s->prefix[0]!='\0')
    {
        if(ipv6_str_to_bin(s->prefix,&s->prefix_bin)==0)
        {
            s->has_prefix_bin=true;
        }
        else
        {
            s->has_prefix_bin=false;
        }
    }
    else
    {
        s->has_prefix_bin=false;
    }
}
static void convert_pd_block(dhcpv6_subnet_t *s)
{
    if(!s->pd_enabled)
    {
        s->has_pd_pool=false;
        return;
    }

    if(s->pd_pool_start[0]!='\0' && s->pd_pool_end[0]!='\0')
    {
        if(ipv6_str_to_bin(s->pd_pool_start,&s->pd_pool_start_bin)==0 &&
           ipv6_str_to_bin(s->pd_pool_end,&s->pd_pool_end_bin)==0)
        {
            s->has_pd_pool=true;
        }
        else
        {
            s->has_pd_pool=false;
        }
    }
    else
    {
        s->has_pd_pool=false;
    }
}

void convert_all_to_binary(dhcpv6_config_t *cfg)
{
    if(!cfg) return;

    for(uint16_t i=0;i<cfg->subnet_count;i++)
    {
        dhcpv6_subnet_t *s = &cfg->subnets[i];
        convert_subnet_prefix(s);
        convert_range_block(s);
        convert_pd_block(s);

        for(uint16_t j=0;j<s->host_count;j++)
        {
            convert_host_block(&s->hosts[j]);
        }
    }

}
int load_config_v6(const char *path, dhcpv6_config_t *cfg)
{
    memset(cfg,0,sizeof(*cfg));

    FILE *fp = fopen(path,"r");
    if(!fp)
    {
        perror("fopen config");
        return -1;
    }

    dhcpv6_subnet_t *current_subnet = NULL;

    char line[LINE_MAX_LEN];
    while(fgets(line,sizeof(line),fp))
    {
        rstrip(line);
        char *p=lskip(line);

        if(*p=='\0' || *p=='#') continue;

        if(*p=='}')
        {
            current_subnet=NULL;
            continue;
        }

        if(starts_with(p,"host") && current_subnet!=NULL)
        {
            if(parse_host_block(fp,current_subnet,p)!=0)
            {
                fclose(fp);
                return -1;
            }
            continue;
        }

        if(current_subnet)
        {
            if(starts_with(p,"range6"))
            {
                parse_range(current_subnet,p);
                continue;
            }
            if(starts_with(p,"prefix6"))
            {
                parse_prefix6(current_subnet,p);
                continue;
            }
            if(starts_with(p,"option dhcp6."))
            {
                parse_subnet_option(current_subnet,p);
                continue;
            }
            continue;
        }

        if(starts_with(p,"subnet6"))
        {
            current_subnet=start_new_subnet(cfg,p);
            if(!current_subnet)
            {
                fclose(fp);
                return -1;
            }
            continue;
        }

        parse_global_option(cfg,p);
    }
    fclose(fp);
    return 0;
}

void dump_config_v6(const dhcpv6_config_t *cfg)
{
    printf("DHCPv6 Configuration:\n");
    printf("Global default lease: %u s\n",cfg->global.default_lease_time);
    printf("Global max lease: %u s\n",cfg->global.max_lease_time);
    printf("Global DNS servers: %s\n",cfg->global.global_dns_servers);
    printf("Global domain search: %s\n",cfg->global.global_domain_search);


    for(uint16_t i=0;i<cfg->subnet_count;i++)
    {
        const dhcpv6_subnet_t *s = &cfg->subnets[i];
        printf("Subnet %u:\n",i+1);
        printf("  Prefix: %s/%u\n",s->prefix,s->prefix_len);
        if(s->has_pool_range)
        {
            printf("  Pool range: %s - %s\n",s->pool_start,s->pool_end);    
        }

        if(s->pd_enabled)
        {
            printf("  PD Pool: %s - %s/%u\n",s->pd_pool_start,s->pd_pool_end,s->pd_prefix_len);
        }

        printf("  DNS servers: %s\n",s->dns_servers);
        printf("  Domain search: %s\n",s->domain_search);
        printf("Hosts: %u\n",s->host_count);
        for(uint16_t j=0;j<s->host_count;j++)
        {
            printf("Host[%u] DUID=%s IP=%s (%s)\n",j,s->hosts[j].duid,
                   s->hosts[j].has_fixed_address6_bin ? s->hosts[j].fixed_address6 : "N/A",s->hosts[j].hostname);
        }
        printf("\n");

    }
}

dhcpv6_subnet_t *find_subnet_for_ipv6(const dhcpv6_config_t *cfg, const struct in6_addr *ip)
{
    for(uint16_t i=0;i<cfg->subnet_count;i++)
    {
        dhcpv6_subnet_t *s = (dhcpv6_subnet_t*)&cfg->subnets[i];
        if(s->has_prefix_bin && ipv6_in_subnet(ip,&s->prefix_bin, s->prefix_len))
        {
            return s;
        }
    }
    return NULL;
}

dhcpv6_static_host_t *find_host_by_duid(dhcpv6_subnet_t *subnet, const char *duid)
{
    for(uint16_t i=0;i<subnet->host_count;i++)
    {
        if(strcmp(subnet->hosts[i].duid,duid)==0)
        {
            return &subnet->hosts[i];
        }
    }
    return NULL;
}