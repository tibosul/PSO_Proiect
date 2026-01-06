#ifndef LEASES6_H
#define LEASES6_H


#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <netinet/in.h>
#include "time_utils.h"


#define LEASES6_MAX 4096
#define DUID_MAX_LEN 128
#define IP6_STR_MAX 80
#define HOSTNAME6_MAX 128
#define LEASE6_PATH_MAX 512
#define LEASE_V6_STR_MAX       96
#define MAX_V6_VENDOR_CLASS_LEN 128
#define MAX_V6_FQDN_LEN 256

typedef enum {
    Lease6_IA_NA = 0,
    Lease6_IA_PD = 1
}lease_v6_type_t;

typedef enum{
    LEASE_STATE_FREE = 0,
    LEASE_STATE_ACTIVE,
    LEASE_STATE_EXPIRED,
    LEASE_STATE_RELEASED,
    LEASE_STATE_ABANDONED,
    LEASE_STATE_RESERVED,
    LEASE_STATE_BACKUP
}lease_state_t;

typedef struct dhcpv6_lease_t{
    uint8_t in_use;
    lease_v6_type_t type;

    //identificare client
    uint8_t duid[DUID_MAX_LEN];
    uint16_t duid_len;
    uint32_t iaid;

    //IA NA
    struct in6_addr ip6_addr;
    char ip6_addr_str[IP6_STR_MAX];

    //IA PD
    struct in6_addr prefix_v6;
    uint8_t plen;
    char prefix_str[IP6_STR_MAX];

    //timpi
    time_t starts;
    time_t ends;
    time_t tstp; //time state was put
    time_t cltt; //client last transaction time

    char client_hostname[HOSTNAME6_MAX];
    lease_state_t state;
    lease_state_t next_state; //next binding state
    lease_state_t rewind_state; //rewind binding state
    char binding_state[16];

    char vendor_class[MAX_V6_VENDOR_CLASS_LEN];
    char fqdn[MAX_V6_FQDN_LEN];

}dhcpv6_lease_t;

typedef struct lease_v6_db_t{
    char filename[LEASE6_PATH_MAX];
    uint32_t count;
    uint32_t capacity;
    dhcpv6_lease_t leases[LEASES6_MAX];
}lease_v6_db_t;

const char* lease_v6_state_to_string(lease_state_t s);
lease_state_t lease_v6_state_from_string(const char* str);
bool lease_v6_is_expired(const dhcpv6_lease_t* lease);

int lease_v6_db_init(lease_v6_db_t *db, const char* filename);
void lease_v6_db_free(lease_v6_db_t *db);

int lease_v6_db_load(lease_v6_db_t *db);
int lease_v6_db_save(lease_v6_db_t *db);
int lease_v6_db_append(lease_v6_db_t *db, const dhcpv6_lease_t *lease);

dhcpv6_lease_t* lease_v6_add_ia_na(lease_v6_db_t *db, const char* duid, uint16_t duid_len, uint32_t iaid, const struct in6_addr* ip6_addr, uint32_t lease_sec, const char* hostname);
dhcpv6_lease_t* lease_v6_add_ia_pd(lease_v6_db_t* db, const char* duid, uint16_t duid_len, uint32_t iaid, const struct in6_addr* prefix_v6, uint8_t plen, uint32_t lease_sec, const char* hostname);

//functii de cautare lease
dhcpv6_lease_t* lease_v6_find_by_ip(lease_v6_db_t *db, const struct in6_addr* ip6_addr);
dhcpv6_lease_t* lease_v6_find_by_prefix(lease_v6_db_t* db, const struct in6_addr* prefix_v6, uint8_t plen);
dhcpv6_lease_t* lease_v6_find_by_duid_iaid(lease_v6_db_t *db, const uint8_t* duid, uint16_t duid_len, uint32_t iaid, lease_v6_type_t type);

int lease_v6_release_ip(lease_v6_db_t* db, const struct in6_addr* ip6_addr);
int lease_v6_release_prefix(lease_v6_db_t* db, const struct in6_addr* prefix_v6, uint8_t plen);
int lease_v6_renew_ip(lease_v6_db_t* db, const struct in6_addr* ip6_addr, uint32_t lease_sec);
int lease_v6_renew_prefix(lease_v6_db_t* db, const struct in6_addr* prefix_v6, uint8_t plen, uint32_t lease_sec);
int lease_v6_mark_expired_older(lease_v6_db_t* db);
int lease_v6_cleanup(lease_v6_db_t* db);
int lease_v6_mark_reserved(lease_v6_db_t* db, const struct in6_addr* ip6, const char* duid_hex, uint32_t iaid, const char* hostname);
void lease_v6_db_print(const lease_v6_db_t* lease);

int lease_v6_set_state(lease_v6_db_t* db, const struct in6_addr* ip6_addr, lease_state_t new_state);
int lease_v6_mark_conflict(lease_v6_db_t* db, const struct in6_addr* ip6_addr, const char* reason); 

#endif // LEASES6_H
