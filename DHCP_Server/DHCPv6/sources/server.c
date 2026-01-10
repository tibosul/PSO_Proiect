#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "logger.h"
#include "config_v6.h"
#include "leases6.h"
#include "ip6_pool.h"
#include "pd_pool.h"
#include "protocol_v6.h"
#include "utilsv6.h"
#include "shm_stats.h"

#define BUF_SIZE 4096
#define THREAD_POOL_SIZE 8
#define QUEUE_SIZE 256

#include "dhcpv6_agent.h"

static volatile int running = 1;

void dhcpv6_agent_stop() {
    running = 0;
}

// --- Thread Pool Structs ---
typedef struct {
    uint8_t buf[BUF_SIZE];
    ssize_t len;
    struct sockaddr_in6 client_addr;
} task_t;

/**
 * @brief Thread-safe task queue for distributing DHCPv6 packets to worker threads.
 */
typedef struct {
    task_t tasks[QUEUE_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t lock;   // Protects queue access
    pthread_cond_t  cond;   // Signals workers when new tasks arrive
    int shutdown;           // Flag to signal server shutdown
} task_queue_t;

// Structs Definition

// Main server context.
// Holds the config, database, IP pools, and the socket/sync primitives.
typedef struct {
    dhcpv6_config_t config;             // Parsed config settings
    lease_v6_db_t   db;                 // Database for persisting leases
    struct ip6_pool_t pools[MAX_SUBNET_V6]; // In-memory bitmaps (fast lookup)
    pd_pool_t       pd_pools[MAX_SUBNET_V6];// Prefix Delegation pools
    int             server_sock;        // Main UDP socket
    pthread_mutex_t     db_lock;        // Big lock for DB and Pool access
    
    // Shared Memory for the Dashboard
    int shm_fd;
    server_stats_t* stats;              // Pointer to shared stats
} server_ctx_t;

static server_ctx_t ctx;
static task_queue_t queue;
static pthread_t threads[THREAD_POOL_SIZE];
static pthread_t cleaner_thread;

// Thread Pool / Task Queue Logic

// Initialize the task queue.
// Just zeroes out indices and sets up the synchronization primitives.
void queue_init(task_queue_t* q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->shutdown = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
}

// Add a new packet to the queue.
// It's thread-safe: locks the queue, copies data, and signals a worker.
void queue_push(task_queue_t* q, const uint8_t* buf, ssize_t len, const struct sockaddr_in6* addr) {
    pthread_mutex_lock(&q->lock);
    if (q->count < QUEUE_SIZE && !q->shutdown) {
        task_t* t = &q->tasks[q->tail];
        memcpy(t->buf, buf, len);
        t->len = len;
        t->client_addr = *addr;
        q->tail = (q->tail + 1) % QUEUE_SIZE;
        q->count++;
        pthread_cond_signal(&q->cond); // Wake up a sleeping worker
    } else {
        log_warn("Queue full or shutting down, dropping packet.");
    }
    pthread_mutex_unlock(&q->lock);
}

// Helpers
dhcpv6_subnet_t* find_subnet(const struct in6_addr* src_ip) {
    if (!src_ip) return &ctx.config.subnets[0];
    if (IN6_IS_ADDR_LINKLOCAL(src_ip)) return &ctx.config.subnets[0];
    
    for (int i=0; i<ctx.config.subnet_count; i++) {
        dhcpv6_subnet_t* s = &ctx.config.subnets[i];
        if (ipv6_in_subnet(src_ip, &s->prefix_bin, s->prefix_len)) {
            return s;
        }
    }
    return &ctx.config.subnets[0];
}

struct ip6_pool_t* get_pool_by_subnet(dhcpv6_subnet_t* sn) {
    int idx = (int)(sn - ctx.config.subnets);
    if (idx >= 0 && idx < ctx.config.subnet_count) return &ctx.pools[idx];
    return NULL;
}

pd_pool_t* get_pd_pool_by_subnet(dhcpv6_subnet_t* sn) {
    int idx = (int)(sn - ctx.config.subnets);
    if (idx >= 0 && idx < ctx.config.subnet_count) return &ctx.pd_pools[idx];
    return NULL;
}

void process_packet(uint8_t* buf, ssize_t len, struct sockaddr_in6* client_addr) {
    dhcpv6_packet_meta_t meta;
    if (dhcpv6_parse(buf, (size_t)len, &meta) != 0) {
        log_warn("Failed to parse packet");
        return;
    }
    
    char src_str[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &client_addr->sin6_addr, src_str, sizeof(src_str));
    
    uint8_t out_buf[BUF_SIZE];
    size_t out_len = 0; // Current offset
    

    pthread_mutex_lock(&ctx.db_lock);
    
    dhcpv6_subnet_t* subnet = find_subnet(&client_addr->sin6_addr);
    struct ip6_pool_t* pool = get_pool_by_subnet(subnet);
    pd_pool_t* pd_pool = get_pd_pool_by_subnet(subnet);
    
    struct in6_addr zero_addr = {0};
    uint8_t server_duid[14] = {0,1,0,1, 0xAA,0xBB,0xCC,0xDD, 0xEE,0xFF, 0x00,0x00}; 
    uint16_t server_duid_len = 14; 
    
    // Determine Message Type
    uint8_t reply_type = 0;
    if (meta.msg_type == MSG_SOLICIT) reply_type = MSG_ADVERTISE;
    else if (meta.msg_type == MSG_REQUEST || meta.msg_type == MSG_RENEW || meta.msg_type == MSG_REBIND || 
             meta.msg_type == MSG_RELEASE || meta.msg_type == MSG_DECLINE) reply_type = MSG_REPLY;
    
    if (reply_type != 0) {
        // Init Header
        if (sizeof(dhcpv6_header_t) > BUF_SIZE) { pthread_mutex_unlock(&ctx.db_lock); return; }
        
        dhcpv6_header_t *hdr = (dhcpv6_header_t *)out_buf;
        hdr->msg_type = reply_type;
        dhcpv6_set_xid(hdr, meta.transaction_id);
        out_len = sizeof(dhcpv6_header_t);
        
        // Append Options
        dhcpv6_append_option(out_buf, BUF_SIZE, &out_len, OPT_SERVERID, server_duid, server_duid_len);
        if (meta.client_duid && meta.client_duid_len > 0) {
            dhcpv6_append_option(out_buf, BUF_SIZE, &out_len, OPT_CLIENTID, meta.client_duid, meta.client_duid_len);
        }

        /* --- Inject Configured Options --- */
        // ... (options appending code remains same) ...
        
        // Handle RELEASE / DECLINE Actions (Pre-processing before building reply)
        if (meta.msg_type == MSG_RELEASE || meta.msg_type == MSG_DECLINE) {
             if (meta.has_ia_na && ctx.db.count > 0) {
                 if (meta.msg_type == MSG_RELEASE) {
                     lease_v6_release_ip(&ctx.db, &meta.requested_ip); 
                 } else {
                     if (pool) ip6_pool_mark_conflict(pool, meta.requested_ip, &ctx.db, "Client Decline");
                 }
             }
             if (meta.has_ia_pd && pd_pool) {
                  if (meta.msg_type == MSG_RELEASE) {
                      pd_pool_release(pd_pool, &meta.requested_prefix, meta.requested_plen, &ctx.db);
                  }
             }
        }

        // ... options ...
        
        // 1. DNS Servers
        const char* dns_str = subnet->dns_servers[0] ? subnet->dns_servers : ctx.config.global.global_dns_servers;
        if (dns_str[0]) {
             struct in6_addr dlist[8];
             int c = str_to_ipv6_list(dns_str, dlist, 8);
             if (c > 0) {
                 ssize_t w = dhcpv6_append_dns_servers(out_buf + out_len, BUF_SIZE - out_len, dlist, c);
                 if (w > 0) out_len += (size_t)w;
             }
        }
        // ...

        // Process IA_NA
        if (meta.has_ia_na && pool) {
             char duid_hex[256];
             duid_bin_to_hex(meta.client_duid, meta.client_duid_len, duid_hex, sizeof(duid_hex));
             
             if (meta.msg_type == MSG_RELEASE || meta.msg_type == MSG_DECLINE) {
                 // Build Reply with Status Success
                   dhcpv6_append_ia_na(out_buf, BUF_SIZE, &out_len, meta.iaid, &meta.requested_ip, 
                                       0, 0, 0, 0, STATUS_SUCCESS);
                if(ctx.stats) __sync_fetch_and_sub(&ctx.stats->leases_active, 1);
             } else {
                 struct ip6_allocation_result_t res = ip6_pool_allocate(pool, duid_hex, meta.client_duid_len,    
                     meta.iaid, NULL, meta.requested_ip, &ctx.config, &ctx.db, subnet->default_lease_time);
                 
                 if (res.success) {
                      dhcpv6_append_ia_na(out_buf, BUF_SIZE, &out_len, meta.iaid, &res.ip_address, 
                                          subnet->default_lease_time, subnet->max_lease_time, 
                                          subnet->default_lease_time, subnet->max_lease_time, STATUS_SUCCESS);
                      if(res.is_new && ctx.stats) __sync_fetch_and_add(&ctx.stats->leases_active, 1);
                 } else {
                      dhcpv6_append_ia_na(out_buf, BUF_SIZE, &out_len, meta.iaid, &zero_addr, 
                                          0, 0, 0, 0, STATUS_NOADDRSAVAIL);
                 }
             }
        }
        
        // Process IA_PD
        if (meta.has_ia_pd && pd_pool) {
             char duid_hex[256];
             duid_bin_to_hex(meta.client_duid, meta.client_duid_len, duid_hex, sizeof(duid_hex));
             
             if (meta.msg_type == MSG_RELEASE || meta.msg_type == MSG_DECLINE) {
                    dhcpv6_append_ia_pd(out_buf, BUF_SIZE, &out_len, meta.iaid_pd, &meta.requested_prefix, meta.requested_plen,
                                      0, 0, 0, 0, STATUS_SUCCESS);
                    if(ctx.stats) __sync_fetch_and_sub(&ctx.stats->leases_active, 1);
             } else {
                 pd_allocation_result_t res = pd_pool_allocate(pd_pool, duid_hex, meta.client_duid_len, 
                                                               meta.iaid_pd, NULL, &ctx.db, subnet->default_lease_time);
                 
                 if (res.success) {
                      dhcpv6_append_ia_pd(out_buf, BUF_SIZE, &out_len, meta.iaid_pd, &res.prefix, res.plen,
                                          subnet->default_lease_time, subnet->max_lease_time, 
                                          subnet->default_lease_time, subnet->max_lease_time, STATUS_SUCCESS);
                      if(res.is_new && ctx.stats) __sync_fetch_and_add(&ctx.stats->leases_active, 1);
                 } else {
                      dhcpv6_append_ia_pd(out_buf, BUF_SIZE, &out_len, meta.iaid_pd, &zero_addr, 0, 
                                          0, 0, 0, 0, STATUS_NOADDRSAVAIL);
                 }
             }
        }
    }
    
    // Done with DB, unlock.
    pthread_mutex_unlock(&ctx.db_lock);
    
    if (out_len > sizeof(dhcpv6_header_t)) {
        char dest_str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &client_addr->sin6_addr, dest_str, sizeof(dest_str));
        log_debug("Sending reply to %s (Scope ID: %d)", dest_str, client_addr->sin6_scope_id);

        if (sendto(ctx.server_sock, out_buf, out_len, 0, (struct sockaddr*)client_addr, sizeof(*client_addr)) < 0) {
            log_warn("Failed to send reply: %s", strerror(errno));
        } else {
            log_info("Reply sent (%ld bytes) to %s", out_len, dest_str);
        }
    }
}

// Cleanup / Garbage Collector Thread.
// Runs once a minute to expire old leases.
void* cleanup_thread(void* arg) {
    (void)arg;
    log_info("Cleanup thread started.");
    
    while(running) {
        sleep(60); // Sleep for 60 seconds
        if (!running) break;
        
        log_debug("Running cleanup...");
        
        pthread_mutex_lock(&ctx.db_lock);
        
        // 1. Mark old leases as expired
        int expired = lease_v6_mark_expired_older(&ctx.db);
        
        // 2. Actually remove them from memory
        if (expired > 0) {
             lease_v6_cleanup(&ctx.db);
        }
        
        // 3. Sync ip pools (bitmaps) with the DB reality
        for(int i=0; i<ctx.config.subnet_count; i++) {
            ip6_pool_sync_with_leases(&ctx.pools[i], &ctx.db);
            // PD pools check DB directly
        }
        
        // 4. Update Dashboard stats (Shared Memory)
        uint64_t active_count = 0;
        for(uint32_t j=0; j<ctx.db.count; j++) {
            if(ctx.db.leases[j].state == LEASE_STATE_ACTIVE) active_count++;
        }
        if(ctx.stats) ctx.stats->leases_active = active_count;

        lease_v6_db_save(&ctx.db); // Persist to disk
        pthread_mutex_unlock(&ctx.db_lock);
    }
    return NULL;
}

void* worker_thread(void* arg) {
    (void)arg;
    while (1) {
        pthread_mutex_lock(&queue.lock);
        while (queue.count == 0 && !queue.shutdown) {
            pthread_cond_wait(&queue.cond, &queue.lock);
        }
        if (queue.shutdown) {
            pthread_mutex_unlock(&queue.lock);
            break;
        }
        
        task_t task = queue.tasks[queue.head];
        queue.head = (queue.head + 1) % QUEUE_SIZE;
        queue.count--;
        
        pthread_mutex_unlock(&queue.lock);
        
        // Removed early increment. Moved to process_packet success path.
        process_packet(task.buf, task.len, &task.client_addr);
    }
    return NULL;
}

// Starts everything up: loads config, inits memory, starts threads.
void* dhcpv6_agent_start(void* arg) {
    (void)arg; 
    
    running = 1; 

    init_logger("[DHCPv6-Server]", LOG_DEBUG, true, "logs/dhcpv6_server.log");
    log_info("Starting DHCPv6 Server (Multi-threaded with %d threads)...", THREAD_POOL_SIZE);
    
    // Load config
    memset(&ctx, 0, sizeof(ctx));
    if (load_config_v6("config/dhcpv6.conf", &ctx.config) != 0) {
        log_error("Failed to load config");
        return NULL;
    }
    convert_all_to_binary(&ctx.config);
    log_info("Config loaded.");
    
    // Init DB usage Mutex
    pthread_mutex_init(&ctx.db_lock, NULL);

    // Init DB
    if (lease_v6_db_init(&ctx.db, "leases/dhcpd6.leases") != 0) {
        log_error("Failed to init lease DB");
        return NULL;
    }
    
    // Init Dashboard (Shared Memory)
    // Create the RAM file for live stats
    ctx.shm_fd = shm_open(SHM_STATS_NAME, O_CREAT | O_RDWR, 0666);
    if (ctx.shm_fd < 0) {
        log_warn("shm_open failed: %s", strerror(errno));
    } else {
        if (ftruncate(ctx.shm_fd, sizeof(server_stats_t)) < 0) {
            log_error("ftruncate failed");
        }
        
        ctx.stats = mmap(NULL, sizeof(server_stats_t), PROT_READ | PROT_WRITE, MAP_SHARED, ctx.shm_fd, 0);
        if (ctx.stats == MAP_FAILED) {
            log_warn("mmap failed: %s", strerror(errno));
            ctx.stats = NULL;
        } else {
            memset(ctx.stats, 0, sizeof(server_stats_t));
            ctx.stats->start_time = time(NULL);
            log_info("Live Dashboard started at %s", SHM_STATS_NAME);
        }
    }
    lease_v6_db_load(&ctx.db);
    log_info("Leases loaded.");
    
    // Init pools
    for (int i=0; i<ctx.config.subnet_count; i++) {
        log_info("Init pool for subnet %d", i);
        ip6_pool_init(&ctx.pools[i], &ctx.config.subnets[i], &ctx.db);
        pd_pool_init(&ctx.pd_pools[i], &ctx.config.subnets[i], &ctx.db, ctx.config.subnets[i].pd_prefix_len);
    }

    // Init Thread Pool
    queue_init(&queue);
    
    for (int i=0; i<THREAD_POOL_SIZE; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread, NULL) != 0) {
            perror("pthread_create worker");
            return NULL;
        }
    }
    log_info("Thread pool initialized with %d threads.", THREAD_POOL_SIZE);
    
    // Start Cleanup Thread
    if (pthread_create(&cleaner_thread, NULL, cleanup_thread, NULL) != 0) {
        perror("pthread_create cleanup");
        return NULL;
    }
    
    // Socket
    ctx.server_sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (ctx.server_sock < 0) {
        perror("socket");
        return NULL;
    }
    
    int on = 1;
    setsockopt(ctx.server_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    setsockopt(ctx.server_sock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
    
    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(DHCPV6_PORT_SERVER);
    addr.sin6_addr = in6addr_any;
    
    if (bind(ctx.server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return NULL;
    }
    
    // Join Multicast Group ff02::1:2 (All DHCPv6 Servers)
    struct ipv6_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));
    inet_pton(AF_INET6, "ff02::1:2", &mreq.ipv6mr_multiaddr);
    mreq.ipv6mr_interface = 0; 
    
    if (setsockopt(ctx.server_sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) < 0) {
        perror("setsockopt(IPV6_JOIN_GROUP)");
    } else {
        log_info("Joined multicast group ff02::1:2");
    }

    log_info("Listening on port %d...", DHCPV6_PORT_SERVER);
    
    uint8_t buf[BUF_SIZE];
    struct pollfd pfd;
    pfd.fd = ctx.server_sock;
    pfd.events = POLLIN;
    
    while (running) {
        int ret = poll(&pfd, 1, 1000); // 1 sec timeout
        
        if (ret < 0) {
           if (errno == EINTR) continue;
           perror("poll");
           break;
        }
        
        if (ret == 0) continue; // Timeout, check running
        
        if (pfd.revents & POLLIN) {
            struct sockaddr_in6 client_addr;
            socklen_t slen = sizeof(client_addr);
            
            ssize_t len = recvfrom(ctx.server_sock, buf, BUF_SIZE, 0, (struct sockaddr*)&client_addr, &slen);
            if (len < 0) {
                if (errno == EINTR) continue;
                perror("recvfrom");
                break;
            }
            
            if (ctx.stats) __sync_fetch_and_add(&ctx.stats->pkt_received, 1);
            
            // Push to Queue instead of processing directly
            queue_push(&queue, buf, len, &client_addr);
        }
    }
    
    // Cleanup
    log_info("Shutting down...");
    
    // Stop threads
    pthread_mutex_lock(&queue.lock);
    queue.shutdown = 1;
    pthread_cond_broadcast(&queue.cond);
    pthread_mutex_unlock(&queue.lock);
    
    for (int i=0; i<THREAD_POOL_SIZE; i++) {
        pthread_join(threads[i], NULL);
    }
    pthread_cancel(cleaner_thread);
    pthread_join(cleaner_thread, NULL);
    
    log_info("Thread pool stopped.");
    
    lease_v6_db_save(&ctx.db);
    lease_v6_db_free(&ctx.db);
    for (int i=0; i<ctx.config.subnet_count; i++) {
        ip6_pool_free(&ctx.pools[i]);
        pd_pool_free(&ctx.pd_pools[i]);
    }
    close(ctx.server_sock);
    pthread_mutex_destroy(&ctx.db_lock);
    
    // Unlink SHM (Cleanup)
    if (ctx.stats) munmap(ctx.stats, sizeof(server_stats_t));
    if (ctx.shm_fd >= 0) { close(ctx.shm_fd); shm_unlink(SHM_STATS_NAME); }
    
    return NULL;
}
