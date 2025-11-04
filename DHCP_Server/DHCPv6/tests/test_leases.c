// main_v6.c — demo minim pentru lease_v6
#include "leases6.h"
#include "logger.h"            // dacă nu ai logger, vezi comentariul de mai jos
#include <arpa/inet.h>

int main(void)
{
    // Inițializează logger-ul tău (adaptează la semnătura ta)
    // log_init((logger_config_t){ .prefix="dhcpv6", .level=LOG_DEBUG, .to_file=1, .path="dhcpv6.log" });

    lease_v6_db_t db;
    // Alege o cale unde ai drept de scriere
    lease_v6_db_init(&db, "./dhcpd6.leases");

    // Încarcă ce există deja (ok și dacă fișierul lipsește)
    lease_v6_db_load(&db);

    // ====== Adaugă o adresă IA_NA ======
    struct in6_addr ip6;
    inet_pton(AF_INET6, "2001:db8:1::1234", &ip6);
    // DUID hex (exemplu DUID-LLT fictiv):
    const char *duid_hex = "00:01:00:01:23:45:67:89:aa:bb";
    lease_v6_add_ia_na(&db, duid_hex, 1 /* IAID */, &ip6, 3600 /*s*/, "laptop-andra");

    // ====== Adaugă un prefix IA_PD ======
    struct in6_addr pfx;
    inet_pton(AF_INET6, "2001:db8:3::", &pfx);
    lease_v6_add_ia_pd(&db, "00:01:00:01:de:ad:be:ef:01:02", 7 /* IAID */,
                       &pfx, 56 /* /56 */, 7200 /*s*/, "router-andra");

    // Poți marca expirate cele vechi, dacă vrei:
    lease_v6_mark_expired_older(&db);

    // Salvează atomic baza (scrie header + toate intrările)
    lease_v6_db_save(&db);

    // Debug: print în consolă
    lease_v6_db_print(&db);

    return 0;
}
