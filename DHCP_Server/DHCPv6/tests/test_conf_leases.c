#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

#include "config_v6.h"
#include "leases6.h"
#include "utilsv6.h"
#include "ip6_pool.h"
// Dacă ai un header diferit pentru logger, ajustează aici:
#include "logger.h"
/* Fallback la printf dacă logger.h are alte nume;
   poți defini în Makefile: CFLAGS+=-DHAVE_LOGGER */
#ifndef HAVE_LOGGER
#  define log_info(fmt, ...)  do { printf("[INFO] " fmt "\n", ##__VA_ARGS__); } while(0)
#  define log_error(fmt, ...) do { fprintf(stderr, "[ERR ] " fmt "\n", ##__VA_ARGS__); } while(0)
#  define log_debug(fmt, ...) do { printf("[DBG ] " fmt "\n", ##__VA_ARGS__); } while(0)
#endif

static void banner(void) {
    puts("============================================================");
    puts("  DHCPv6 – Test config & leases");
    puts("============================================================");
}



static uint32_t pick_lease_time(const dhcpv6_config_t* cfg, const dhcpv6_subnet_t* sn) {
    if (sn && sn->max_lease_time) return sn->max_lease_time;
    if (sn && sn->default_lease_time) return sn->default_lease_time;
    if (cfg->global.max_lease_time) return cfg->global.max_lease_time;
    if (cfg->global.default_lease_time) return cfg->global.default_lease_time;
    return 3600;
}

int main(void)
{
    banner();

    /* 1) Init logger */
#ifdef HAVE_LOGGER
    /* Ajustează semnătura dacă loggerul tău e diferit */
    init_logger("DHCPv6", LOG_INFO, 1 /*to_file*/, "dhcpv6_test.log");
#endif
    log_info("Logger initialized");

    /* 2) Încarcă config-ul */
    const char *conf_path = "config/dhcpv6.conf";
    dhcpv6_config_t* cfg=malloc(sizeof *cfg);
    if(!cfg)
    {
        perror("malloc");
        return -1;
    }
    if (load_config_v6(conf_path, cfg) != 0) {
        log_error("Failed to load config: %s", conf_path);
        return 1;
    }
    log_info("Config loaded from %s", conf_path);
    convert_all_to_binary(cfg);
    int errors = validate_config_v6(cfg);
    if(errors>0)
    {
        printf("Eroare la validare");
        return -1;
    }
    dump_config_v6(cfg);

   
    /* 3) Init DB lease-uri */
    const char *lease_path = "leases/dhcpd6.leases";
    lease_v6_db_t db;
    if (lease_v6_db_init(&db, lease_path) != 0) {
        log_error("lease_v6_db_init failed for %s", lease_path);
        return 2;
    }
    log_info("Lease DB initialized: %s", lease_path);

    /* 4) Încarcă lease-urile existente (dacă fișierul există) */
    if (lease_v6_db_load(&db) == 0) {
        log_info("Lease DB loaded (count=%u)", db.count);
    } else {
        log_error("Lease DB load returned error (continuing with empty DB)");
    }

    /* 5) (OPȚIONAL) Adaugă un IA_NA și un IA_PD ca test */
    struct in6_addr ip6;
    inet_pton(AF_INET6, "2001:db8:1::10", &ip6);

    const char *duid_hex = "00:01:00:01:aa:bb:cc:dd:ee:ff:11:22";
    uint16_t duid_len = 0; /* Dacă API-ul tău cere duid_len, îl calculează intern din hex */
    (void)duid_len;        /* liniștește -Wunused */

    /* IA_NA – adresă unică */
    dhcpv6_lease_t *L1 = lease_v6_add_ia_na(
        &db,
        duid_hex,
        0 /* duid_len – dacă funcția îl ignoră */,
        1234 /* IAID */,
        &ip6,
        3600 /* secunde */,
        "laptop-andra"
    );
    if (L1) {
        log_info("Added IA_NA lease for %s", "2001:db8:1::10");
    } else {
        log_error("Failed to add IA_NA lease");
    }

    /* IA_PD – delegare de prefix /56 ca exemplu */
    struct in6_addr pfx;
    inet_pton(AF_INET6, "2001:db8:3::", &pfx);
    dhcpv6_lease_t *L2 = lease_v6_add_ia_pd(
        &db,
        "00:01:00:01:de:ad:be:ef:01:02:03:04",
        0 /* duid_len – idem */,
        5678 /* IAID */,
        &pfx,
        56 /* prefix length */,
        7200 /* secunde */,
        "router-andra"
    );
    if (L2) {
        log_info("Added IA_PD lease for %s/%u", "2001:db8:3::", 56u);
    } else {
        log_error("Failed to add IA_PD lease");
    }

    /* 6) Salvează DB pe disc */
    if (lease_v6_db_save(&db) != 0) {
        log_error("lease_v6_db_save failed");
        return 3;
    }
    log_info("Lease DB saved to %s", lease_path);

    /* 7) Printează în consolă (debug) */
    lease_v6_db_print(&db);

    if (cfg->subnet_count == 0) {
        log_error("No IPv6 subnets configured — cannot init pool");
        return -1;
    }
    dhcpv6_subnet_t* sn = &cfg->subnets[0];
    struct ip6_pool_t pool;
    if (ip6_pool_init(&pool, sn, &db) != 0) {
        log_error("ip6_pool_init failed");
        return -1;
    }
    ip6_pool_print_stats(&pool);
    const char* test_duid = "00:01:00:01:aa:bb:cc:dd:ee:ff:11:33";
    uint32_t test_iaid = 4242;
    const char* test_hn = "tester-v6";

    /* opțional: încearcă să ceri explicit o adresă; dacă nu, pune :: (zero) */
    struct in6_addr req = {0};
    inet_pton(AF_INET6, "2001:db8:1::20", &req); /* decomentează dacă vrei requested IP */

    uint32_t lease_time = pick_lease_time(cfg, sn);

    struct ip6_allocation_result_t ar =
        ip6_pool_allocate(&pool, test_duid, test_iaid, test_hn, req, cfg, &db, lease_time);

    if (!ar.success) {
        if (ar.err_is_conflict) {
            char cip[INET6_ADDRSTRLEN]; inet_ntop(AF_INET6, &ar.conflict_ip, cip, sizeof(cip));
            log_error("Allocation failed: conflict on %s (%s)", cip, ar.conflict_reason?ar.conflict_reason:"probe");
        } else {
            log_error("Allocation failed: %s", ar.error_message);
        }
        /* nu ies din test — vreau să văd dump-urile măcar */
    } else {
        char ipstr[INET6_ADDRSTRLEN]; inet_ntop(AF_INET6, &ar.ip_address, ipstr, sizeof(ipstr));
        log_info("Allocated IPv6 address: %s", ipstr);
    }

    /* 7) Salvează DB pe disc și arată detalii */
    if (lease_v6_db_save(&db) != 0) {
        log_error("lease_v6_db_save failed");
        return 3;
    }
    ip6_pool_print_detailed(&pool);
    lease_v6_db_print(&db);

    log_info("Done.");
    free(cfg);
    return 0;
}
