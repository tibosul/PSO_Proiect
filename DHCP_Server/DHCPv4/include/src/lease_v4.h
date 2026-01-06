#ifndef LEASE_V4_H
#define LEASE_V4_H

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>

#define MAX_LEASES 1024
#define MAX_CLIENT_HOSTNAME 256
#define MAX_CLIENT_ID_LEN 64
#define MAX_VENDOR_CLASS_LEN 128

typedef enum
{
    LEASE_STATE_FREE = 0,  // Available for allocation
    LEASE_STATE_ACTIVE,    // Currently leased
    LEASE_STATE_EXPIRED,   // Lease time expired
    LEASE_STATE_RELEASED,  // Client released the lease
    LEASE_STATE_ABANDONED, // Ping check failed or other issue
    LEASE_STATE_RESERVED,  // Static/reserved (from config)
    LEASE_STATE_BACKUP,    // Backup state for failover
    LEASE_STATE_UNKNOWN    // Unknown (used as return value to avoid warnings)
} lease_state_t;

/**
 * @brief Convert lease state enum to string.
 * @param state Lease state enumeration value.
 * @return String representation (e.g., "active", "free", "expired").
 */
const char *lease_state_to_string(lease_state_t state);

/**
 * @brief Convert string to lease state enum.
 * @param str State string (e.g., "active", "free").
 * @return Lease state enumeration value.
 */
lease_state_t lease_state_from_string(const char *str);

struct dhcp_lease_t
{
    uint64_t lease_id; // Unique lease identifier (never changes)
    struct in_addr ip_address;
    uint8_t mac_address[6];

    // Timestamps
    time_t start_time; // Unix timestamp - when lease started (starts)
    time_t end_time;   // Unix timestamp - when lease expires (ends)
    time_t tstp;       // Time State was Put - last state change
    time_t cltt;       // Client Last Transaction Time

    // State management
    lease_state_t state;                // Current state
    lease_state_t next_binding_state;   // State after expiration
    lease_state_t rewind_binding_state; // State to revert to on failure

    // Client identification
    uint8_t client_id[MAX_CLIENT_ID_LEN];
    uint32_t client_id_len;
    char client_hostname[MAX_CLIENT_HOSTNAME];

    // Vendor information (Option 60)
    char vendor_class_identifier[MAX_VENDOR_CLASS_LEN];

    // Flags
    bool is_abandoned; // Marked as abandoned (ping failed)
    bool is_bootp;     // BOOTP client (no expiration)

    // For backward compatibility with file format
    char binding_state[32]; // String representation of state
};

struct lease_database_t
{
    struct dhcp_lease_t leases[MAX_LEASES];
    uint32_t lease_count;
    char filename[256];     // Path to lease file
    uint64_t next_lease_id; // Counter for generating unique IDs

    // Thread safety
    pthread_mutex_t db_mutex;     // Protects access to leases[] and lease_count
    bool mutex_initialized;       // Track if mutex was initialized
};

/**
 * @brief Timer thread for automatic lease expiration checks.
 *
 * This structure manages a background thread that periodically checks
 * for expired leases and marks them as EXPIRED automatically.
 */
struct lease_timer_t
{
    struct lease_database_t *db;  // Pointer to the lease database
    pthread_t timer_thread;       // Timer thread handle
    bool running;                 // Thread running flag
    uint32_t check_interval_sec;  // How often to check (e.g., 60 seconds)

    // Synchronization
    pthread_mutex_t timer_mutex;  // Protects timer state
    pthread_cond_t timer_cond;    // For early wake-up and shutdown
    bool mutex_initialized;       // Track if mutex was initialized
};

/**
 * @brief I/O queue operation type.
 */
typedef enum
{
    IO_OP_SAVE_LEASE,   // Append single lease to file
    IO_OP_SAVE_ALL,     // Save entire database to file
    IO_OP_SHUTDOWN      // Shutdown signal
} io_operation_type_t;

/**
 * @brief I/O queue operation.
 */
struct io_operation_t
{
    io_operation_type_t type;
    struct dhcp_lease_t lease;  // Lease data (for SAVE_LEASE)
    time_t timestamp;           // When operation was queued
};

#define IO_QUEUE_SIZE 256

/**
 * @brief Async I/O queue for non-blocking disk writes.
 *
 * This structure manages a background thread that handles all disk I/O
 * asynchronously, preventing the main thread from blocking on write operations.
 * Uses producer-consumer pattern with circular buffer.
 */
struct lease_io_queue_t
{
    struct lease_database_t *db;  // Pointer to the lease database
    pthread_t io_thread;          // I/O thread handle
    bool running;                 // Thread running flag

    // Circular queue for pending operations
    struct io_operation_t queue[IO_QUEUE_SIZE];
    uint32_t head;                // Queue head (consumer reads here)
    uint32_t tail;                // Queue tail (producer writes here)
    uint32_t count;               // Number of items in queue

    // Synchronization
    pthread_mutex_t queue_mutex;  // Protects queue state
    pthread_cond_t queue_cond;    // Signals when items available
    bool mutex_initialized;       // Track if mutex was initialized

    // Statistics
    uint64_t operations_processed;
    uint64_t operations_dropped;  // When queue is full
};

// ----------------------------------------------------------------------------------------------

/**
 * @brief Initialize a lease database.
 * @param db Pointer to the lease database structure.
 * @param filename Path to the lease file.
 * @return 0 on success, -1 on failure.
 */
int lease_db_init(struct lease_database_t *db, const char *filename);

/**
 * @brief Free resources associated with the lease database.
 * @param db Pointer to the lease database structure.
 */
void lease_db_free(struct lease_database_t *db);


/**
 * @brief Load leases from the lease file into the database.
 * @param db Pointer to the lease database structure.
 * @return 0 on success, -1 on failure.
 *
 * Reads the ISC DHCP compatible lease file and populates the lease database.
 * If the file doesn't exist, initializes an empty database.
 * Automatically generates lease IDs for backward compatibility.
 */
int lease_db_load(struct lease_database_t *db);

/**
 * @brief Save all leases to the lease file.
 * @param db Pointer to the lease database structure.
 * @return 0 on success, -1 on failure.
 *
 * Writes the entire lease database to disk in ISC DHCP format.
 * This is a full rewrite of the file (not append).
 */
int lease_db_save(struct lease_database_t *db);

/**
 * @brief Append a single lease to the lease file.
 * @param db Pointer to the lease database structure.
 * @param lease Pointer to the lease to append.
 * @return 0 on success, -1 on failure.
 *
 * Appends a lease entry to the end of the lease file without rewriting
 * the entire file. More efficient for real-time lease updates.
 */
int lease_db_append_lease(struct lease_database_t *db, const struct dhcp_lease_t *lease);

/**
 * @brief Generate next unique lease ID.
 * @param db Pointer to the lease database structure.
 * @return A unique 64-bit lease ID, or 0 on failure.
 *
 * Generates monotonically increasing lease IDs. These IDs are stable
 * and never change for a lease, even if the lease is renewed.
 *
 * Thread-safe: Uses atomic operations internally, safe to call without holding db_mutex.
 * Note: lease_db_load() should be called before any concurrent ID generation to avoid conflicts.
 */
uint64_t lease_db_generate_id(struct lease_database_t *db);

/**
 * @brief Find a lease by its unique ID.
 * @param db Pointer to the lease database structure.
 * @param lease_id The unique lease identifier to search for.
 * @return Pointer to the lease if found, NULL otherwise.
 *
 * Provides stable reference to a lease using its immutable ID.
 * Lease IDs never change, unlike IP addresses or MAC addresses.
 */
struct dhcp_lease_t *lease_db_find_by_id(struct lease_database_t *db, uint64_t lease_id);

/**
 * @brief Add a new lease to the database.
 * @param db Pointer to the lease database structure.
 * @param ip IP address to lease.
 * @param mac Client MAC address (6 bytes).
 * @param lease_time Lease duration in seconds.
 * @return Pointer to the newly created lease, or NULL on failure.
 *
 * Creates a new lease with ACTIVE state. Automatically sets start_time
 * to now and calculates end_time based on lease_time.
 *
 * Note: Does NOT automatically persist to disk. Caller must call lease_db_append_lease(),
 * lease_db_save(), or use the I/O queue (lease_io_queue_save_lease) to persist changes.
 * This function must be called with db_mutex held in multi-threaded contexts.
 */
struct dhcp_lease_t *lease_db_add_lease(struct lease_database_t *db, struct in_addr ip, const uint8_t mac[6], uint32_t lease_time);

/**
 * @brief Find a lease by IP address.
 * @param db Pointer to the lease database structure.
 * @param ip IP address to search for.
 * @return Pointer to the lease if found, NULL otherwise.
 */
struct dhcp_lease_t *lease_db_find_by_ip(struct lease_database_t *db, struct in_addr ip);

/**
 * @brief Find a lease by MAC address.
 * @param db Pointer to the lease database structure.
 * @param mac MAC address to search for (6 bytes).
 * @return Pointer to the lease if found, NULL otherwise.
 *
 * Returns the first active lease for this MAC address.
 * A client may have multiple leases in different states.
 */
struct dhcp_lease_t *lease_db_find_by_mac(struct lease_database_t *db, const uint8_t mac[6]);

/**
 * @brief Release a lease (mark as FREE).
 * @param db Pointer to the lease database structure.
 * @param ip IP address of the lease to release.
 * @return 0 on success, -1 on failure.
 *
 * Called when a client sends DHCPRELEASE. Transitions the lease
 * to FREE state and updates timestamps.
 *
 * Note: Does NOT automatically persist to disk. Caller must save changes manually.
 * This function must be called with db_mutex held in multi-threaded contexts.
 */
int lease_db_release_lease(struct lease_database_t *db, struct in_addr ip);

/**
 * @brief Renew an existing lease.
 * @param db Pointer to the lease database structure.
 * @param ip IP address of the lease to renew.
 * @param lease_time New lease duration in seconds.
 * @return 0 on success, -1 on failure.
 *
 * Extends the lease expiration time. Updates cltt (client last
 * transaction time) and recalculates end_time.
 *
 * Note: Does NOT automatically persist to disk. Caller must save changes manually.
 * This function must be called with db_mutex held in multi-threaded contexts.
 */
int lease_db_renew_lease(struct lease_database_t *db, struct in_addr ip, uint32_t lease_time);

/**
 * @brief Mark expired leases as EXPIRED.
 * @param db Pointer to the lease database structure.
 * @return Number of leases expired.
 *
 * Scans all active leases and transitions expired ones to EXPIRED state.
 * Should be called periodically by the DHCP server main loop.
 *
 * Note: Does NOT automatically persist to disk. Caller must save changes manually.
 * This function must be called with db_mutex held in multi-threaded contexts.
 */
int lease_db_expire_old_leases(struct lease_database_t *db);

/**
 * @brief Remove expired leases from the database.
 * @param db Pointer to the lease database structure.
 * @return Number of leases removed.
 *
 * Permanently removes leases in EXPIRED or FREE state to reclaim memory.
 * Use cautiously - this deletes lease history.
 *
 * Note: Does NOT automatically persist to disk. Caller must save changes manually.
 * This function must be called with db_mutex held in multi-threaded contexts.
 */
int lease_db_cleanup_expired(struct lease_database_t *db);

/**
 * @brief Print lease database to stdout (for debugging).
 * @param db Pointer to the lease database structure.
 *
 * Displays all leases with their state, IP, MAC, and expiration times.
 * Useful for debugging and monitoring.
 */
void lease_db_print(const struct lease_database_t *db);

/**
 * @brief Check if a lease has expired.
 * @param lease Pointer to the lease structure.
 * @return true if expired, false otherwise.
 *
 * Compares end_time with current time. Only checks ACTIVE leases.
 */
bool lease_is_expired(const struct dhcp_lease_t *lease);

/**
 * @brief Set client identifier (DHCP option 61) for a lease.
 * @param lease Pointer to the lease structure.
 * @param client_id Client identifier bytes.
 * @param len Length of client_id.
 * @return 0 on success, -1 on failure.
 *
 * Stores the client identifier sent in DHCP option 61.
 * Used for more reliable client identification than MAC address.
 */
int lease_set_client_id(struct dhcp_lease_t *lease, const uint8_t *client_id, uint32_t len);

/**
 * @brief Set vendor class identifier (DHCP option 60) for a lease.
 * @param lease Pointer to the lease structure.
 * @param vendor_class Vendor class string.
 * @return 0 on success, -1 on failure.
 *
 * Stores vendor identification (e.g., "MSFT 5.0", "Cisco Systems").
 * Useful for applying vendor-specific configurations.
 */
int lease_set_vendor_class(struct dhcp_lease_t *lease, const char *vendor_class);

/**
 * @brief Update lease timestamps to current time.
 * @param lease Pointer to the lease structure.
 * @param now Current Unix timestamp.
 *
 * Updates tstp (time state was put) and cltt (client last transaction time).
 */
void lease_update_timestamps(struct dhcp_lease_t *lease, time_t now);

/**
 * @brief Set state transition for a lease.
 * @param lease Pointer to the lease structure.
 * @param current Current binding state.
 * @param next Next binding state (after expiration).
 * @param rewind Rewind binding state (on failure/rollback).
 *
 * Configures the state machine for lease transitions.
 * Example: current=ACTIVE, next=FREE, rewind=FREE
 */
void lease_set_state_transition(struct dhcp_lease_t *lease, lease_state_t current, lease_state_t next, lease_state_t rewind);

/**
 * @brief Parse client ID from string format to binary.
 * @param str Client ID string (e.g., "\\001\\000\\021\\042").
 * @param client_id Output buffer for binary client ID.
 * @param len Output length of parsed client ID.
 * @return 0 on success, -1 on failure.
 *
 * Parses ISC DHCP lease file format for client identifiers.
 * Handles both hex escape sequences and quoted strings.
 */
int parse_client_id_from_string(const char *str, uint8_t *client_id, uint32_t *len);

/**
 * @brief Format client ID from binary to string representation.
 * @param client_id Binary client identifier.
 * @param len Length of client_id.
 * @param output Output buffer for formatted string.
 * @param output_len Size of output buffer.
 *
 * Formats client ID for writing to lease file in ISC DHCP format.
 */
void format_client_id_to_string(const uint8_t *client_id, uint32_t len, char *output, size_t output_len);

/**
 * @brief Parse timestamp from ISC DHCP lease file format.
 * @param time_str Time string (e.g., "4 2024/10/26 14:30:00").
 * @return Unix timestamp, or 0 on parse error.
 *
 * Parses ISC DHCP date format: <weekday> <YYYY/MM/DD> <HH:MM:SS>
 * Example: "4 2024/10/26 14:30:00" or "epoch 1698334200"
 */
time_t parse_lease_time(const char *time_str);

/**
 * @brief Format Unix timestamp to ISC DHCP lease file format.
 * @param timestamp Unix timestamp to format.
 * @param output Output buffer for formatted time string.
 * @param output_len Size of output buffer.
 *
 * Formats timestamp as: <weekday> <YYYY/MM/DD> <HH:MM:SS>
 * Example output: "4 2024/10/26 14:30:00"
 */
void format_lease_time(time_t timestamp, char *output, size_t output_len);

// ----------------------------------------------------------------------------------------------
// Thread-Safe Operations (Paso 1: Mutex Protection)
// ----------------------------------------------------------------------------------------------

/**
 * @section Thread-Safety Guidelines
 *
 * This lease database API provides two types of functions:
 *
 * 1. **Non-Safe Functions** (e.g., lease_db_add_lease, lease_db_find_by_ip):
 *    - Fast, direct access to database
 *    - Do NOT perform automatic I/O operations (for performance)
 *    - MUST be called with db_mutex held in multi-threaded contexts
 *    - Return pointers to internal data (valid only while lock is held)
 *    - Use when you need to perform multiple operations atomically
 *
 * 2. **Safe Functions** (e.g., lease_db_add_lease_safe, lease_db_find_by_ip_safe):
 *    - Automatically acquire and release db_mutex
 *    - Return copies of data (safe to use after function returns)
 *    - Simpler to use but have more overhead
 *    - Use for single, isolated operations
 *
 * @subsection Usage Patterns
 *
 * **Pattern 1: Using Safe Functions (Recommended for Simple Operations)**
 * @code
 * // Find a lease safely
 * struct dhcp_lease_t lease_copy;
 * if (lease_db_find_by_ip_safe(db, ip, &lease_copy) == 0) {
 *     // Use lease_copy - it's a copy, safe to use without holding lock
 *     printf("Lease state: %s\n", lease_state_to_string(lease_copy.state));
 * }
 * @endcode
 *
 * **Pattern 2: Using Non-Safe Functions for Atomic Operations**
 * @code
 * // Multiple operations under one lock
 * lease_db_lock(db);
 * struct dhcp_lease_t *lease = lease_db_find_by_ip(db, ip);
 * if (lease && lease->state == LEASE_STATE_ACTIVE) {
 *     lease_db_release_lease(db, ip);
 *     // Persist changes (outside critical section if using I/O queue)
 * }
 * lease_db_unlock(db);
 * @endcode
 *
 * **Pattern 3: Persisting Changes**
 * @code
 * // After modifying data, persist it:
 *
 * // Option A: Async I/O queue (recommended, non-blocking)
 * lease_io_queue_save_lease(io_queue, &lease);
 *
 * // Option B: Synchronous save (blocks until written)
 * lease_db_save_safe(db);
 * @endcode
 *
 * @warning IMPORTANT: Non-safe functions do NOT automatically save to disk.
 *          Always persist changes manually using I/O queue or lease_db_save().
 *
 * @warning NEVER hold db_mutex while performing I/O operations directly.
 *          Use the I/O queue (lease_io_queue_*) for async disk writes.
 *
 * @note lease_db_load() should be called once at initialization, before
 *       starting any worker threads, to avoid race conditions with ID generation.
 */

/**
 * @brief Lock the lease database for exclusive access.
 * @param db Pointer to the lease database structure.
 *
 * Must be called before accessing lease data from multiple threads.
 * Always pair with lease_db_unlock().
 */
void lease_db_lock(struct lease_database_t *db);

/**
 * @brief Unlock the lease database.
 * @param db Pointer to the lease database structure.
 *
 * Must be called after lease_db_lock() to release the lock.
 */
void lease_db_unlock(struct lease_database_t *db);

/**
 * @brief Thread-safe version of lease_db_add_lease.
 * @param db Pointer to the lease database structure.
 * @param ip IP address to lease.
 * @param mac Client MAC address (6 bytes).
 * @param lease_time Lease duration in seconds.
 * @param out_lease Output buffer to copy the newly created lease data (optional, can be NULL).
 * @return Unique lease ID on success, 0 on failure.
 *
 * Automatically locks/unlocks the database during operation.
 * Returns a copy of the lease data to avoid holding references to internal memory after unlock.
 */
uint64_t lease_db_add_lease_safe(struct lease_database_t *db, struct in_addr ip, const uint8_t mac[6],
                                 uint32_t lease_time, struct dhcp_lease_t *out_lease);

/**
 * @brief Thread-safe version of lease_db_find_by_ip.
 * @param db Pointer to the lease database structure.
 * @param ip IP address to search for.
 * @param out_lease Output buffer to copy the lease data.
 * @return 0 on success (lease found), -1 on failure (not found).
 *
 * Copies lease data to out_lease to avoid holding the lock.
 * Automatically locks/unlocks the database during operation.
 */
int lease_db_find_by_ip_safe(struct lease_database_t *db, struct in_addr ip, struct dhcp_lease_t *out_lease);

/**
 * @brief Thread-safe version of lease_db_find_by_mac.
 * @param db Pointer to the lease database structure.
 * @param mac MAC address to search for (6 bytes).
 * @param out_lease Output buffer to copy the lease data.
 * @return 0 on success (lease found), -1 on failure (not found).
 *
 * Copies lease data to out_lease to avoid holding the lock.
 * Automatically locks/unlocks the database during operation.
 */
int lease_db_find_by_mac_safe(struct lease_database_t *db, const uint8_t mac[6], struct dhcp_lease_t *out_lease);

/**
 * @brief Thread-safe version of lease_db_find_by_id.
 * @param db Pointer to the lease database structure.
 * @param lease_id The unique lease identifier to search for.
 * @param out_lease Output buffer to copy the lease data.
 * @return 0 on success (lease found), -1 on failure (not found).
 *
 * Copies lease data to out_lease to avoid holding the lock.
 * Automatically locks/unlocks the database during operation.
 */
int lease_db_find_by_id_safe(struct lease_database_t *db, uint64_t lease_id, struct dhcp_lease_t *out_lease);

/**
 * @brief Thread-safe version of lease_db_release_lease.
 * @param db Pointer to the lease database structure.
 * @param ip IP address of the lease to release.
 * @return 0 on success, -1 on failure.
 *
 * Automatically locks/unlocks the database during operation.
 */
int lease_db_release_lease_safe(struct lease_database_t *db, struct in_addr ip);

/**
 * @brief Thread-safe version of lease_db_renew_lease.
 * @param db Pointer to the lease database structure.
 * @param ip IP address of the lease to renew.
 * @param lease_time New lease duration in seconds.
 * @return 0 on success, -1 on failure.
 *
 * Automatically locks/unlocks the database during operation.
 */
int lease_db_renew_lease_safe(struct lease_database_t *db, struct in_addr ip, uint32_t lease_time);

/**
 * @brief Thread-safe version of lease_db_expire_old_leases.
 * @param db Pointer to the lease database structure.
 * @return Number of leases expired.
 *
 * Automatically locks/unlocks the database during operation.
 */
int lease_db_expire_old_leases_safe(struct lease_database_t *db);

/**
 * @brief Thread-safe version of lease_db_cleanup_expired.
 * @param db Pointer to the lease database structure.
 * @return Number of leases removed.
 *
 * Automatically locks/unlocks the database during operation.
 */
int lease_db_cleanup_expired_safe(struct lease_database_t *db);

/**
 * @brief Thread-safe version of lease_db_save.
 * @param db Pointer to the lease database structure.
 * @return 0 on success, -1 on failure.
 *
 * Automatically locks/unlocks the database during operation.
 */
int lease_db_save_safe(struct lease_database_t *db);

// ----------------------------------------------------------------------------------------------
// Timer Thread Operations (Paso 2: Automatic Expiration)
// ----------------------------------------------------------------------------------------------

/**
 * @brief Initialize a lease timer for automatic expiration checks.
 * @param timer Pointer to the lease_timer_t structure.
 * @param db Pointer to the lease database to monitor.
 * @param check_interval_sec How often to check for expired leases (in seconds).
 * @return 0 on success, -1 on failure.
 *
 * Initializes the timer structure but does not start the thread.
 * Call lease_timer_start() to begin automatic expiration checks.
 */
int lease_timer_init(struct lease_timer_t *timer, struct lease_database_t *db, uint32_t check_interval_sec);

/**
 * @brief Start the timer thread.
 * @param timer Pointer to the lease_timer_t structure.
 * @return 0 on success, -1 on failure.
 *
 * Starts a background thread that periodically checks for expired leases.
 * The thread runs until lease_timer_stop() is called.
 */
int lease_timer_start(struct lease_timer_t *timer);

/**
 * @brief Stop the timer thread and clean up resources.
 * @param timer Pointer to the lease_timer_t structure.
 *
 * Gracefully stops the timer thread and waits for it to terminate.
 * Destroys all synchronization primitives.
 */
void lease_timer_stop(struct lease_timer_t *timer);

/**
 * @brief Wake up the timer thread immediately.
 * @param timer Pointer to the lease_timer_t structure.
 *
 * Forces the timer thread to wake up and check for expired leases now,
 * instead of waiting for the next scheduled check.
 */
void lease_timer_wakeup(struct lease_timer_t *timer);

/**
 * @brief Check if the timer thread is running.
 * @param timer Pointer to the lease_timer_t structure.
 * @return true if running, false otherwise.
 */
bool lease_timer_is_running(const struct lease_timer_t *timer);

// ----------------------------------------------------------------------------------------------
// I/O Queue Operations (Paso 3: Async Disk I/O)
// ----------------------------------------------------------------------------------------------

/**
 * @brief Initialize the I/O queue for async disk operations.
 * @param io_queue Pointer to the lease_io_queue_t structure.
 * @param db Pointer to the lease database.
 * @return 0 on success, -1 on failure.
 *
 * Initializes the I/O queue structure but does not start the thread.
 * Call lease_io_start() to begin processing operations.
 */
int lease_io_init(struct lease_io_queue_t *io_queue, struct lease_database_t *db);

/**
 * @brief Start the I/O thread.
 * @param io_queue Pointer to the lease_io_queue_t structure.
 * @return 0 on success, -1 on failure.
 *
 * Starts a background thread that processes disk I/O operations from the queue.
 */
int lease_io_start(struct lease_io_queue_t *io_queue);

/**
 * @brief Stop the I/O thread and clean up resources.
 * @param io_queue Pointer to the lease_io_queue_t structure.
 *
 * Gracefully stops the I/O thread, processes remaining operations,
 * and destroys all synchronization primitives.
 */
void lease_io_stop(struct lease_io_queue_t *io_queue);

/**
 * @brief Queue a lease for async save (append to file).
 * @param io_queue Pointer to the lease_io_queue_t structure.
 * @param lease Pointer to the lease to save.
 * @return 0 on success, -1 on failure (queue full).
 *
 * Non-blocking: adds the lease to the queue and returns immediately.
 * The I/O thread will append it to the lease file in the background.
 */
int lease_io_queue_save_lease(struct lease_io_queue_t *io_queue, const struct dhcp_lease_t *lease);

/**
 * @brief Queue a full database save operation.
 * @param io_queue Pointer to the lease_io_queue_t structure.
 * @return 0 on success, -1 on failure (queue full).
 *
 * Non-blocking: queues a complete database save and returns immediately.
 * The I/O thread will perform the save in the background.
 */
int lease_io_queue_save_all(struct lease_io_queue_t *io_queue);

/**
 * @brief Get I/O queue statistics.
 * @param io_queue Pointer to the lease_io_queue_t structure.
 * @param processed Output: number of operations processed.
 * @param dropped Output: number of operations dropped (queue full).
 * @param pending Output: number of operations currently pending.
 */
void lease_io_get_stats(struct lease_io_queue_t *io_queue, uint64_t *processed, uint64_t *dropped, uint32_t *pending);

/**
 * @brief Check if I/O queue is running.
 * @param io_queue Pointer to the lease_io_queue_t structure.
 * @return true if running, false otherwise.
 */
bool lease_io_is_running(const struct lease_io_queue_t *io_queue);

// ----------------------------------------------------------------------------------------------
// Unified Server Structure (Paso 4: Signal Handling & Server Management)
// ----------------------------------------------------------------------------------------------

/**
 * @brief Unified DHCP server structure combining all components.
 *
 * This structure brings together the lease database, timer thread, and I/O queue
 * into a single manageable unit with signal handling support.
 */
struct dhcp_server_t
{
    // Core components
    struct lease_database_t *lease_db;     // Lease database
    struct lease_timer_t *timer;           // Expiration timer thread
    struct lease_io_queue_t *io_queue;     // Async I/O queue

    // Signal handling
    volatile sig_atomic_t shutdown_requested;  // Set by signal handler
    volatile sig_atomic_t reload_requested;    // Set by SIGHUP

    // Synchronization for shutdown
    pthread_mutex_t server_mutex;
    pthread_cond_t server_cond;
    bool mutex_initialized;
};

/**
 * @brief Initialize the DHCP server with all components.
 * @param server Pointer to the dhcp_server_t structure.
 * @param lease_file Path to the lease database file.
 * @param timer_interval Expiration check interval in seconds (0 to disable timer).
 * @param enable_async_io Enable async I/O queue (true/false).
 * @return 0 on success, -1 on failure.
 *
 * Initializes all server components but does not start threads.
 * Call dhcp_server_start() to begin operation.
 */
int dhcp_server_init(struct dhcp_server_t *server, const char *lease_file,
                     uint32_t timer_interval, bool enable_async_io);

/**
 * @brief Start all server components.
 * @param server Pointer to the dhcp_server_t structure.
 * @return 0 on success, -1 on failure.
 *
 * Starts timer thread and I/O queue (if enabled).
 */
int dhcp_server_start(struct dhcp_server_t *server);

/**
 * @brief Stop all server components and clean up.
 * @param server Pointer to the dhcp_server_t structure.
 *
 * Gracefully stops all threads, saves pending data, and frees resources.
 */
void dhcp_server_stop(struct dhcp_server_t *server);

/**
 * @brief Setup signal handlers for the server.
 * @param server Pointer to the dhcp_server_t structure.
 *
 * Installs signal handlers for:
 * - SIGINT (Ctrl+C): Graceful shutdown
 * - SIGTERM: Graceful shutdown
 * - SIGHUP: Reload configuration (sets reload flag)
 */
void dhcp_server_setup_signals(struct dhcp_server_t *server);

/**
 * @brief Wait for shutdown signal.
 * @param server Pointer to the dhcp_server_t structure.
 *
 * Blocks until a shutdown signal is received (SIGINT or SIGTERM).
 * Returns when shutdown_requested flag is set.
 */
void dhcp_server_wait_for_shutdown(struct dhcp_server_t *server);

/**
 * @brief Check if reload was requested.
 * @param server Pointer to the dhcp_server_t structure.
 * @return true if SIGHUP was received, false otherwise.
 *
 * Also clears the reload flag after checking.
 */
bool dhcp_server_check_reload(struct dhcp_server_t *server);

/**
 * @brief Print server statistics.
 * @param server Pointer to the dhcp_server_t structure.
 *
 * Displays information about lease count, I/O queue stats, etc.
 */
void dhcp_server_print_stats(const struct dhcp_server_t *server);

#endif // LEASE_V4_H