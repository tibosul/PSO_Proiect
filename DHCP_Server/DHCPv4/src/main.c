#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
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
#include "../../logger/logger.h"

// Constants
#define RECV_BUF_SIZE 1024
#define SERVER_CONFIG_FILE "DHCPv4/config/dhcpv4.conf"
#define LEASE_DB_FILE "DHCPv4/data/dhcpv4.leases"

// Global state
static volatile int g_running = 1;
struct server_context_t
{
    int sockfd;
    struct dhcp_server_t dhcp;  // Unified lease management (db + timer + I/O queue)
    struct dhcp_config_t config;
    struct ip_pool_t pools[MAX_SUBNETS];
    struct ip_pool_sync_t pool_syncs[MAX_SUBNETS];
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
        log_warn("Received invalid DHCP packet");
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

    log_info("Processing DHCP %s from %s (MAC: %02x:%02x:%02x:%02x:%02x:%02x)",
           msg_type == DHCP_DISCOVER ? "DISCOVER" :
           msg_type == DHCP_REQUEST ? "REQUEST" :
           msg_type == DHCP_RELEASE ? "RELEASE" : "UNKNOWN",
           inet_ntoa(dest.sin_addr),
           req->chaddr[0], req->chaddr[1], req->chaddr[2],
           req->chaddr[3], req->chaddr[4], req->chaddr[5]);

    switch (msg_type)
    {
    case DHCP_DISCOVER:
    {
        struct dhcp_lease_t *lease = NULL;
        // 1. Try to find existing lease for MAC
        lease = lease_db_find_by_mac(g_server.dhcp.lease_db, req->chaddr);

        // 2. Or allocate new IP
        if (!lease)
        {
            struct in_addr req_ip = {0};
            uint8_t *req_ip_opt = dhcp_message_get_option(req, DHCP_OPT_REQUESTED_IP, NULL);
            if (req_ip_opt)
                memcpy(&req_ip.s_addr, req_ip_opt, 4);

            lease = ip_pool_allocate_and_create_lease(
                pool, g_server.dhcp.lease_db, req->chaddr, req_ip, &g_server.config,
                subnet->default_lease_time);
        }

        if (lease)
        {
            dhcp_message_make_offer(&res, req, lease, subnet, &g_server.config.global);
            // Send OFFER
            if (req->giaddr.s_addr != 0)
            {
                dest.sin_port = htons(DHCP_CLIENT_PORT);
                dest.sin_addr = req->giaddr; // Unicast to relay
            }
            else if ((ntohl(task->client_addr.sin_addr.s_addr) & 0xFF000000) == 0x7F000000)
            {
                // Loopback - keep original client address AND port for unicast reply
                dest.sin_port = task->client_addr.sin_port;
            }
            else
            {
                dest.sin_port = htons(DHCP_CLIENT_PORT);
                dest.sin_addr.s_addr = INADDR_BROADCAST; // Broadcast
            }

            sendto(g_server.sockfd, &res, sizeof(res), 0, (struct sockaddr *)&dest, sizeof(dest));
            char ip_buf[INET_ADDRSTRLEN];   
            inet_ntop(AF_INET, &lease->ip_address, ip_buf, sizeof(ip_buf));
            log_info(">>> OFFER: Allocated IP %s to client (lease %us)", ip_buf, subnet->default_lease_time);
        }
        else
        {
            log_warn(">>> OFFER FAILED: No IP available for client");
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
            /*
            if (server_id.s_addr != 0 && server_id != our_server_ip)
            {
                log_warn("DHCPREQUEST SERVER ID does not match this server");
                break;
            }
            */

            lease = lease_db_find_by_ip(g_server.dhcp.lease_db, req_ip);
            if (lease && memcmp(lease->mac_address, req->chaddr, 6) == 0)
            {
                // Confirm lease
                lease_db_renew_lease(g_server.dhcp.lease_db, lease->ip_address, subnet->default_lease_time);
                dhcp_message_make_ack(&res, req, lease, subnet, &g_server.config.global);

                if (req->giaddr.s_addr != 0)
                {
                    dest.sin_port = htons(DHCP_CLIENT_PORT);
                    dest.sin_addr = req->giaddr;
                }
                else if (ip_is_loopback(task->client_addr.sin_addr))
                {
                    // Loopback - keep original client address AND port
                    dest.sin_port = task->client_addr.sin_port;
                }
                else
                {
                    dest.sin_port = htons(DHCP_CLIENT_PORT);
                    dest.sin_addr.s_addr = INADDR_BROADCAST;
                }

                sendto(g_server.sockfd, &res, sizeof(res), 0, (struct sockaddr *)&dest, sizeof(dest));
                char ack_ip_buf[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &lease->ip_address, ack_ip_buf, sizeof(ack_ip_buf));
                log_info(">>> ACK: Confirmed IP %s to client", ack_ip_buf);
            }
            else
            {
                // Send NAK
                dhcp_message_make_nak(&res, req, subnet->router); // Use router IP as server ID
                if (ip_is_loopback(task->client_addr.sin_addr))
                {
                    // Loopback - keep original client address AND port
                    dest.sin_port = task->client_addr.sin_port;
                }
                else
                {
                    dest.sin_port = htons(DHCP_CLIENT_PORT);
                    dest.sin_addr.s_addr = INADDR_BROADCAST;
                }
                sendto(g_server.sockfd, &res, sizeof(res), 0, (struct sockaddr *)&dest, sizeof(dest));
                log_info("Sent DHCPNAK for IP %s", inet_ntoa(req_ip));
            }
        }
        // Renewing / Rebinding (Request IP but no Server ID)
        else if (req->ciaddr.s_addr != 0)
        {
            lease = lease_db_find_by_ip(g_server.dhcp.lease_db, req->ciaddr);
            if (lease)
            {
                lease_db_renew_lease(g_server.dhcp.lease_db, lease->ip_address, subnet->default_lease_time);
                dhcp_message_make_ack(&res, req, lease, subnet, &g_server.config.global);

                // For loopback testing, keep original port; otherwise use standard port
                if (ip_is_loopback(task->client_addr.sin_addr))
                    dest.sin_port = task->client_addr.sin_port;
                else
                    dest.sin_port = htons(DHCP_CLIENT_PORT);
                dest.sin_addr = req->ciaddr; // Unicast to client
                sendto(g_server.sockfd, &res, sizeof(res), 0, (struct sockaddr *)&dest, sizeof(dest));
                log_info("Sent DHCPACK (renewal) for IP %s to %s:%d", inet_ntoa(lease->ip_address),
                         inet_ntoa(dest.sin_addr), ntohs(dest.sin_port));
            }
        }
        break;
    }

    case DHCP_RELEASE:
        if (req->ciaddr.s_addr != 0)
        {
            lease_db_release_lease(g_server.dhcp.lease_db, req->ciaddr);
            ip_pool_release_ip(pool, req->ciaddr);
            log_info("Released IP %s", inet_ntoa(req->ciaddr));
        }
        break;

    default:
        log_warn("Unhandled message type: %d", msg_type);
        break;
    }

    free(task);
}

int main(int argc, char *argv[])
{
    // Initialize logger first - logs to file dhcpv4_server.log
    init_logger("[DHCPv4-Server]", LOG_DEBUG, true, "logs/dhcpv4_server.log");

    log_info("Starting DHCPv4 Server...");
    log_info("Usage: %s [config_file] [interface]", argc > 0 ? argv[0] : "dhcpv4_server");
    log_info("  config_file: path to config (default: %s)", SERVER_CONFIG_FILE);
    log_info("  interface: bind to specific interface (e.g., vmnet1, eth0)");

    // 1. Initialize Signal Handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // 2. Load Configuration
    const char *config_file = SERVER_CONFIG_FILE;
    if (argc > 1)
        config_file = argv[1];

    if (parse_config_file(config_file, &g_server.config) != 0)
    {
        log_error("Failed to load configuration");
        close_logger();
        return 1;
    }
    print_config(&g_server.config);

    // 3. Initialize DHCP server (lease DB + timer + I/O queue)
    // Timer interval: 60 seconds, async I/O: enabled
    if (dhcp_server_init(&g_server.dhcp, LEASE_DB_FILE, 60, true) != 0)
    {
        log_error("Failed to initialize DHCP server");
        close_logger();
        return 1;
    }

    // 4. Initialize IP Pools for each subnet
    g_server.pool_count = g_server.config.subnet_count;
    for (int i = 0; i < g_server.pool_count; i++)
    {
        ip_pool_init(&g_server.pools[i], &g_server.config.subnets[i], g_server.dhcp.lease_db);
    }

    // 5. Initialize and start IP Pool sync threads (sync every 30 seconds)
    for (int i = 0; i < g_server.pool_count; i++)
    {
        if (ip_pool_sync_init(&g_server.pool_syncs[i], &g_server.pools[i], g_server.dhcp.lease_db, 30) == 0)
        {
            ip_pool_sync_start(&g_server.pool_syncs[i]);
        }
    }

    // 6. Start DHCP server (timer thread + I/O queue thread)
    if (dhcp_server_start(&g_server.dhcp) != 0)
    {
        log_error("Failed to start DHCP server threads");
        close_logger();
        return 1;
    }

    // 7. Initialize Worker Thread Pool
    struct thread_pool_t *tpool = thread_pool_create(4, 1024);
    if (!tpool)
    {
        log_error("Failed to create thread pool");
        close_logger();
        return 1;
    }
    log_info("Thread pool initialized with 4 workers");

    // 8. Bind Socket
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

    // Bind to specific interface if provided (argv[2])
    // This is CRITICAL for systems with multiple interfaces to ensure
    // broadcast replies go out the correct interface
    if (argc > 2)
    {
        const char *interface = argv[2];
        if (setsockopt(g_server.sockfd, SOL_SOCKET, SO_BINDTODEVICE,
                       interface, strlen(interface) + 1) < 0)
        {
            log_error("Failed to bind to interface %s: %s", interface, strerror(errno));
            log_info("Note: SO_BINDTODEVICE requires root/CAP_NET_RAW");
            close(g_server.sockfd);
            close_logger();
            return 1;
        }
        log_info("Socket bound to interface: %s", interface);
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DHCP_SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(g_server.sockfd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        log_warn("Failed to bind port 67: %s (needs root/CAP_NET_BIND_SERVICE)", strerror(errno));
        log_info("Trying to bind to non-privileged port 6767 for testing...");
        server_addr.sin_port = htons(6767); // Fallback
        if (bind(g_server.sockfd, (struct sockaddr *)&server_addr,
                 sizeof(server_addr)) < 0)
        {
            log_error("Failed to bind fallback port 6767: %s", strerror(errno));
            close_logger();
            return 1;
        }
    }
    log_info("Server listening on port %d...", ntohs(server_addr.sin_port));

    // 9. Main Loop
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
            log_warn("Failed to add task to pool (queue full), dropping packet");
            free(task);
        }
    }

    // 10. Cleanup
    log_info("Shutting down...");
    thread_pool_destroy(tpool, 0);
    close(g_server.sockfd);

    // Stop sync threads first
    for (int i = 0; i < g_server.pool_count; i++)
    {
        ip_pool_sync_stop(&g_server.pool_syncs[i]);
    }

    // Free IP pools
    for (int i = 0; i < g_server.pool_count; i++)
    {
        ip_pool_free(&g_server.pools[i]);
    }

    // Stop DHCP server (stops timer, I/O queue, saves & frees lease DB)
    dhcp_server_stop(&g_server.dhcp);

    log_info("Server stopped.");
    close_logger();
    return 0;
}
