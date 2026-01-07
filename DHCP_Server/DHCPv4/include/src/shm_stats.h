#ifndef SHM_STATS_V4
#define SHM_STAT_V4

#include <stdint.h>
#include <time.h>

#define SHM_STATS_V4_NAME "/dhcpv4_stats"

/**
 * @brief Shared Memory Statistics Structure.
 * This structure is mapped into memory by both Server (RW) and Monitor (RO).
 */
struct server_v4_stats_t
{
    time_t start_time; // timestamp when server started

    volatile uint64_t pkt_received;     // Total UDP packets received
    volatile uint64_t pkt_processed;    // Valid DHCPv4 packets processed
    volatile uint64_t leases_active;    // Current number of active leases (NA + PD)
    volatile uint64_t errors_count;     // Total error events logged
};

#endif // SHM_STATS_V4