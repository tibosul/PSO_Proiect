#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "../include/src/shm_stats.h"

void clrscr()
{
    printf("\033[H\033[J");
}

int main()
{
    // 1. Open the shared memory read-only (we only want to watch not edit)
    int fd = shm_open(SHM_STATS_V4_NAME, O_RDONLY | O_CREAT, 0666);
    if (fd < 0)
    {
        fprintf(stderr, "Failed to open SHM '%s'. Is server running?\nError: %s\n", 
                SHM_STATS_V4_NAME, strerror(errno));
        return -1;
    }

    // 2. Map it to our memory space
    // Now 'stats' points directly to the server's live data in RAM.
    struct server_v4_stats_t* stats = mmap(NULL, sizeof(struct server_v4_stats_t), PROT_READ, MAP_SHARED, fd, 0);
    if (stats == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        return -1;
    }

    printf("Connected to DHCPv4 Server Dashboard.\n");
    sleep(1);

    while(1)
    {
        clrscr();
        time_t now = time(NULL);
        double uptime = difftime(now, stats->start_time);
        
        printf("========================================\n");
        printf("   DHCPv4 Server Live Dashboard (SHM)   \n");
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

    munmap(stats, sizeof(struct server_v4_stats_t));
    close(fd);
    return 0;
}