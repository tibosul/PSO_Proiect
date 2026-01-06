#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "../include/src/config_v4.h"
#include "../include/src/dhcp_common.h"
#include "../include/src/dhcp_message.h"
#include "../include/src/ip_pool.h"
#include "../include/src/lease_v4.h"
#include "../include/utils/network_utils.h"
#include "../include/utils/thread_pool.h"

// Constants
#define RECV_BUF_SIZE 1024
#define SERVER_CONFIG_FILE "config/dhcpv4.conf"
#define LEASE_DB_FILE "data/dhcpd.leases"

// Global state
static volatile int g_running = 1;
struct server_context_t
{
    int sockfd;
    struct lease_database_t lease_db;
    struct dhcp_config_t config;
    struct ip_pool_t pools[MAX_SUBNETS];
    int pool_count;
};

struct server_context_t g_server;

// Signal handler
void handle_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

// Task argument structure
struct packet_task_t
{
    struct dhcp_packet packet;
    ssize_t len;
    struct sockaddr_in client_addr;
};

// Worker function to process DHCP packets
void packet_processor(void *arg)
{
    struct packet_task_t *task = (struct packet_task_t *)arg;
    struct dhcp_packet *req = &task->packet;
    struct dhcp_packet res;
    struct sockaddr_in dest = task->client_addr;

    // Validate packet
    if (dhcp_message_validate(req, task->len) != 0)
    {
        printf("Received invalid DHCP packet\n");
        free(task);
        return;
    }

    uint8_t msg_type = dhcp_message_get_type(req);

    // Identify subnet based on GIADDR or Interface (simplified to assume local
    // subnet) For a real server, we'd match the interface IP or giaddr to a
    // subnet in config
    struct dhcp_subnet_t *subnet = &g_server.config.subnets[0]; // Naive: use first subnet
    struct ip_pool_t *pool = &g_server.pools[0];

    // Find correct subnet/pool if multiple exist
    // Implementation omitted for brevity - would check if giaddr matches a subnet

    printf("Processing DHCP message type %d from %s\n", msg_type,
           inet_ntoa(dest.sin_addr));

    switch (msg_type)
    {
    case DHCP_DISCOVER:
    {
        struct dhcp_lease_t *lease = NULL;
        // 1. Try to find existing lease for MAC
        lease = lease_db_find_by_mac(&g_server.lease_db, req->chaddr);

        // 2. Or allocate new IP
        if (!lease)
        {
            struct in_addr req_ip = {0};
            uint8_t *req_ip_opt = dhcp_message_get_option(req, DHCP_OPT_REQUESTED_IP, NULL);
            if (req_ip_opt)
                memcpy(&req_ip.s_addr, req_ip_opt, 4);

            lease = ip_pool_allocate_and_create_lease(
                pool, &g_server.lease_db, req->chaddr, req_ip, &g_server.config,
                subnet->default_lease_time);
        }

        if (lease)
        {
            dhcp_message_make_offer(&res, req, lease, subnet,
                                    &g_server.config.global);
            // Send OFFER
            dest.sin_port = htons(DHCP_CLIENT_PORT);
            if (req->giaddr.s_addr != 0)
                dest.sin_addr = req->giaddr; // Unicast to relay
            else
                dest.sin_addr.s_addr = INADDR_BROADCAST; // Broadcast

            sendto(g_server.sockfd, &res, sizeof(res), 0, (struct sockaddr *)&dest,
                   sizeof(dest));
            printf("Sent DHCPOFFER for IP %s\n", inet_ntoa(lease->ip_address));
        }
        break;
    }

    case DHCP_REQUEST:
    {
        struct dhcp_lease_t *lease = NULL;
        struct in_addr req_ip = {0}, server_id = {0};
        uint8_t *req_ip_opt = dhcp_message_get_option(req, DHCP_OPT_REQUESTED_IP, NULL);
        uint8_t *sid_opt = dhcp_message_get_option(req, DHCP_OPT_SERVER_ID, NULL);

        if (req_ip_opt)
            memcpy(&req_ip.s_addr, req_ip_opt, 4);
        if (sid_opt)
            memcpy(&server_id.s_addr, sid_opt, 4);

        // If selecting (SERVER ID present)
        if (sid_opt)
        {
            // Verify we are the server
            // (skip check for now or match against our IP)

            lease = lease_db_find_by_ip(&g_server.lease_db, req_ip);
            if (lease && memcmp(lease->mac_address, req->chaddr, 6) == 0)
            {
                // Confirm lease
                lease_db_renew_lease(&g_server.lease_db, lease->ip_address,
                                     subnet->default_lease_time);
                dhcp_message_make_ack(&res, req, lease, subnet,
                                      &g_server.config.global);

                dest.sin_port = htons(DHCP_CLIENT_PORT);
                if (req->giaddr.s_addr != 0)
                    dest.sin_addr = req->giaddr;
                else
                    dest.sin_addr.s_addr = INADDR_BROADCAST;

                sendto(g_server.sockfd, &res, sizeof(res), 0, (struct sockaddr *)&dest,
                       sizeof(dest));
                printf("Sent DHCPACK for IP %s\n", inet_ntoa(lease->ip_address));
            }
            else
            {
                // Send NAK
                dhcp_message_make_nak(&res, req,
                                      subnet->router); // Use router IP as server ID
                dest.sin_port = htons(DHCP_CLIENT_PORT);
                dest.sin_addr.s_addr = INADDR_BROADCAST;
                sendto(g_server.sockfd, &res, sizeof(res), 0, (struct sockaddr *)&dest,
                       sizeof(dest));
                printf("Sent DHCPNAK for IP %s\n", inet_ntoa(req_ip));
            }
        }
        // Renewing / Rebinding (Request IP but no Server ID)
        else if (req->ciaddr.s_addr != 0)
        {
            lease = lease_db_find_by_ip(&g_server.lease_db, req->ciaddr);
            if (lease)
            {
                lease_db_renew_lease(&g_server.lease_db, lease->ip_address,
                                     subnet->default_lease_time);
                dhcp_message_make_ack(&res, req, lease, subnet,
                                      &g_server.config.global);

                dest.sin_port = htons(DHCP_CLIENT_PORT);
                dest.sin_addr = req->ciaddr; // Unicast to client
                sendto(g_server.sockfd, &res, sizeof(res), 0, (struct sockaddr *)&dest,
                       sizeof(dest));
            }
        }
        break;
    }

    case DHCP_RELEASE:
        if (req->ciaddr.s_addr != 0)
        {
            lease_db_release_lease(&g_server.lease_db, req->ciaddr);
            ip_pool_release_ip(pool, req->ciaddr);
            printf("Released IP %s\n", inet_ntoa(req->ciaddr));
        }
        break;

    default:
        printf("Unhandled message type: %d\n", msg_type);
        break;
    }

    free(task);
}

int main(int argc, char *argv[])
{
    printf("Starting DHCPv4 Server...\n");

    // 1. Initialize Signal Handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // 2. Load Configuration
    const char *config_file = SERVER_CONFIG_FILE;
    if (argc > 1)
        config_file = argv[1];

    if (parse_config_file(config_file, &g_server.config) != 0)
    {
        fprintf(stderr, "Failed to load configuration\n");
        return 1;
    }
    print_config(&g_server.config);

    // 3. Initialize Lease DB
    if (lease_db_init(&g_server.lease_db, LEASE_DB_FILE) != 0)
    {
        fprintf(stderr, "Failed to initialize lease database\n");
        return 1;
    }
    lease_db_load(&g_server.lease_db);

    // 4. Initialize IP Pools for each subnet
    g_server.pool_count = g_server.config.subnet_count;
    for (int i = 0; i < g_server.pool_count; i++)
    {
        ip_pool_init(&g_server.pools[i], &g_server.config.subnets[i],
                     &g_server.lease_db);
    }

    // 5. Initialize Thread Pool
    struct thread_pool_t *tpool = thread_pool_create(4, 1024);
    if (!tpool)
    {
        fprintf(stderr, "Failed to create thread pool\n");
        return 1;
    }
    printf("Thread pool initialized with 4 workers\n");

    // 6. Bind Socket
    if ((g_server.sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket");
        return 1;
    }

    int broadcast_on = 1;
    setsockopt(g_server.sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast_on,
               sizeof(broadcast_on));

    int reuse_addr = 1;
    setsockopt(g_server.sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr,
               sizeof(reuse_addr));

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DHCP_SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(g_server.sockfd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind"); // Note: Needs root or capability to bind port 67
        printf("Trying to bind to non-privileged port 6767 for testing...\n");
        server_addr.sin_port = htons(6767); // Fallback
        if (bind(g_server.sockfd, (struct sockaddr *)&server_addr,
                 sizeof(server_addr)) < 0)
        {
            perror("bind fallback");
            return 1;
        }
    }
    printf("Server listening on port %d...\n", ntohs(server_addr.sin_port));

    // 7. Main Loop
    while (g_running)
    {
        struct packet_task_t *task = malloc(sizeof(struct packet_task_t));
        if (!task)
        {
            usleep(10000); // Backoff on OOM
            continue;
        }

        socklen_t addr_len = sizeof(task->client_addr);
        task->len = recvfrom(g_server.sockfd, &task->packet, sizeof(struct dhcp_packet), 0,
                             (struct sockaddr *)&task->client_addr, &addr_len);

        if (task->len < 0)
        {
            free(task);
            if (errno == EINTR)
                continue;
            perror("recvfrom");
            break;
        }

        // Dispatch to thread pool
        if (thread_pool_add(tpool, packet_processor, task) != 0)
        {
            fprintf(stderr, "Failed to add task to pool (full?)\n");
            free(task);
        }
    }

    // 8. Cleanup
    printf("\nShutting down...\n");
    thread_pool_destroy(tpool, 0);
    close(g_server.sockfd);

    for (int i = 0; i < g_server.pool_count; i++)
    {
        ip_pool_free(&g_server.pools[i]);
    }
    lease_db_free(&g_server.lease_db);

    printf("Server stopped.\n");
    return 0;
}
