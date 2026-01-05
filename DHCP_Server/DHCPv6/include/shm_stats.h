#ifndef SHM_STATS_H
#define SHM_STATS_H

#include <stdint.h>
#include <time.h>

#define SHM_STATS_NAME "/dhcpv6_stats"

/**
 * @brief Shared Memory Statistics Structure.
 * This structure is mapped into memory by both Server (RW) and Monitor (RO).
 */
typedef struct {
    time_t start_time;                  //Timestamp when server started
    
    volatile uint64_t pkt_received;     // Total UDP packets received
    volatile uint64_t pkt_processed;    // Valid DHCPv6 packets processed
    volatile uint64_t leases_active;    // Current number of active leases (NA + PD)
    volatile uint64_t errors_count;     // Total error events logged
} server_stats_t;

#endif // SHM_STATS_H
