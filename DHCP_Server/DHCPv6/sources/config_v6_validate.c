#include "config_v6.h"
#include "utilsv6.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

/*
 * Validate the parsed DHCPv6 configuration.
 * Performs basic logical and range consistency checks:
 *   - subnets have valid prefix_len and non-empty ranges
 *   - range6 lies within the subnet
 *   - PD pool range lies within its declared prefix
 *   - fixed-address6 host bindings belong to their subnet
 *   - start <= end for ranges and PDs
 * Non-fatal issues are reported as warnings.
 */

/* Compare IPv6 lexicographically (memcmp semantics, but explicit). */
static int ip6_compare(const struct in6_addr *a, const struct in6_addr *b)
{
    return memcmp(a->s6_addr, b->s6_addr, 16);
}

static void validate_range6(const dhcpv6_subnet_t *s)
{
    if (!s->has_pool_range) return;

    if (ip6_compare(&s->pool_start_bin, &s->pool_end_bin) > 0) {
        fprintf(stderr, "[WARN] Subnet %s/%u: pool start > pool end\n",
                s->prefix, s->prefix_len);
    }

    if (s->has_prefix_bin && !ipv6_in_subnet(&s->pool_start_bin, &s->prefix_bin, s->prefix_len)) {
        fprintf(stderr, "[WARN] Subnet %s/%u: pool start not in subnet\n",
                s->prefix, s->prefix_len);
    }
    if (s->has_prefix_bin && !ipv6_in_subnet(&s->pool_end_bin, &s->prefix_bin, s->prefix_len)) {
        fprintf(stderr, "[WARN] Subnet %s/%u: pool end not in subnet\n",
                s->prefix, s->prefix_len);
    }
}

static void validate_pd_pool(const dhcpv6_subnet_t *s)
{
    if (!s->pd_enabled || !s->has_pd_pool) return;

    if (ip6_compare(&s->pd_pool_start_bin, &s->pd_pool_end_bin) > 0) {
        fprintf(stderr, "[WARN] Subnet %s/%u: PD pool start > end\n",
                s->prefix, s->prefix_len);
    }

    if (s->has_prefix_bin && !ipv6_in_subnet(&s->pd_pool_start_bin, &s->prefix_bin, s->prefix_len)) {
        fprintf(stderr, "[WARN] Subnet %s/%u: PD start not in prefix\n",
                s->prefix, s->prefix_len);
    }
    if (s->has_prefix_bin && !ipv6_in_subnet(&s->pd_pool_end_bin, &s->prefix_bin, s->prefix_len)) {
        fprintf(stderr, "[WARN] Subnet %s/%u: PD end not in prefix\n",
                s->prefix, s->prefix_len);
    }
}

static void validate_hosts(const dhcpv6_subnet_t *s)
{
    for (uint16_t j = 0; j < s->host_count; j++) {
        const dhcpv6_static_host_t *h = &s->hosts[j];

        if (h->has_fixed_address6_bin && s->has_prefix_bin) {
            if (!ipv6_in_subnet(&h->fixed_addr6_bin, &s->prefix_bin, s->prefix_len)) {
                fprintf(stderr,
                        "[WARN] Subnet %s/%u: host '%s' has fixed-address6=%s not in subnet range\n",
                        s->prefix, s->prefix_len, h->hostname, h->fixed_address6);
            }
        }

        if (!h->duid[0]) {
            fprintf(stderr,
                    "[WARN] Subnet %s/%u: host '%s' missing DUID identifier\n",
                    s->prefix, s->prefix_len, h->hostname);
        }
    }
}

/* Master validator entry point */
int validate_config_v6(const dhcpv6_config_t *cfg)
{
    int errors = 0;

    if (cfg->subnet_count == 0) {
        fprintf(stderr, "[ERROR] No subnet6 definitions found in configuration\n");
        errors++;
    }

    for (uint16_t i = 0; i < cfg->subnet_count; i++) {
        const dhcpv6_subnet_t *s = &cfg->subnets[i];

        if (!s->has_prefix_bin) {
            fprintf(stderr, "[ERROR] Subnet %u has invalid or missing prefix\n", i + 1);
            errors++;
            continue;
        }

        if (s->prefix_len == 0 || s->prefix_len > 128) {
            fprintf(stderr, "[ERROR] Subnet %s/%u invalid prefix length\n", s->prefix, s->prefix_len);
            errors++;
        }

        /* Each subnet’s checks */
        validate_range6(s);
        validate_pd_pool(s);
        validate_hosts(s);
    }

    if (errors == 0)
        fprintf(stderr, "[INFO] DHCPv6 configuration validated successfully.\n");
    else
        fprintf(stderr, "[INFO] DHCPv6 validation completed with %d error(s).\n", errors);

    return errors;
}
