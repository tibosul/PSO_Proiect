#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>

#include "../include/shm_stats.h"

void clrscr() {
    printf("\033[H\033[J");
}

int main() {
    // 1. Open the Shared Memory
    // Read-Only because we just want to watch, not touch.
    int fd = shm_open(SHM_STATS_NAME, O_RDONLY, 0666);
    if (fd < 0) {
        fprintf(stderr, "Failed to open SHM '%s'. Is server running?\nError: %s\n", 
                SHM_STATS_NAME, strerror(errno));
        return 1;
    }

    // 2. Map it to our memory space
    // Now 'stats' points directly to the server's live data in RAM.
    server_stats_t* stats = mmap(NULL, sizeof(server_stats_t), PROT_READ, MAP_SHARED, fd, 0);
    if (stats == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    printf("Connected to DHCPv6 Server Dashboard.\n");
    sleep(1);

    while(1) {
        clrscr();
        time_t now = time(NULL);
        double uptime = difftime(now, stats->start_time);
        
        printf("========================================\n");
        printf("   DHCPv6 Server Live Dashboard (SHM)   \n");
        printf("========================================\n");
        printf("Uptime:          %.0f sec\n", uptime);
        printf("Start Time:      %s", ctime(&stats->start_time));
        printf("----------------------------------------\n");
        printf("Packets RX:      %lu\n", stats->pkt_received);
        printf("Packets Proc:    %lu\n", stats->pkt_processed);
        printf("Active Leases:   %lu\n", stats->leases_active);
        printf("Errors:          %lu\n", stats->errors_count);
        printf("========================================\n");
        printf("Press Ctrl+C to exit monitor.\n");
        
        sleep(1);
    }

    munmap(stats, sizeof(server_stats_t));
    close(fd);
    return 0;
}
