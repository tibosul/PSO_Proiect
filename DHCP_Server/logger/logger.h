#ifndef LOGGER_H
#define LOGGER_H
/*
 * Features
 * - Levels: DEBUG, INFO, WARN, ERROR
 * - Output to stdout/stderr or to file (append mode)
 * - Timestamp in UTC (YYYY-MM-DD HH:MM:SS)
 * - Optional static prefix (e.g., "[DHCPv6]")
 * - Thread-safe (POSIX mutex)
 *
 * Usage
 *   init_logger("[DHCPv6]", LOG_INFO, true, "/var/log/dhcpv6.log");
 *   log_info("Server started on port %u", 547);
 *   log_error("Failed to bind: %s", strerror(errno));
 *   close_logger();
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/** Logging levels in ascending order of severity */
typedef enum
{
    LOG_DEBUG = 0,  /* Very verbose*/
    LOG_INFO = 1,   /* High-level progress/info */
    LOG_WARN = 2,   /* Recoverable problems, unexpected states */
    LOG_ERROR = 3   /* Failures/conditions requiring attention */
} log_level_t;


/**
 * init_logger
 * Initializes the global logger instance.
 *
 * @param prefix   Optional short tag printed before level (e.g., "[DHCPv6]").
 *                 Pass NULL or "" to omit.
 * @param level    Minimum level to print (messages below are dropped).
 * @param to_file  If true, write to file (append); otherwise stdout/stderr.
 * @param path     File path used only when to_file==true. Must be non-NULL and non-empty.
 * @return 0 on success, -1 on error (e.g., cannot open file).
 *
 * Thread-safety: safe to call concurrently, the function locks internally.
 * Re-initializing closes any previously opened file descriptor.
 */
int init_logger(const char *prefix, log_level_t level, bool to_file, const char *path);

/**
 * log_msg
 * Core logging function (use macros below for convenience).
 *
 * @param level    Message level.
 * @param format   printf-style format string.
 * @param ...      Arguments for format.
 *
 * Notes:
 * - Full printf formatting is supported (via vsnprintf).
 * - Thread-safe.
 */
void log_msg(log_level_t level, const char *format, ...);

/**
 * log_set_level / log_get_level
 * Change/query the global minimum level. Messages below this are dropped.
 */
void log_set_level(log_level_t level);

/**
 * close_logger
 * Close resources (file descriptor) and mark logger as uninitialized.
 *
 * Safe to call multiple times.
 */
void close_logger();
log_level_t log_get_level();


/** Convenience macros for readability and zero varargs overhead at call site */
#define log_error(...) log_msg(LOG_ERROR, __VA_ARGS__)
#define log_warn(...)  log_msg(LOG_WARN,  __VA_ARGS__)
#define log_info(...)  log_msg(LOG_INFO,  __VA_ARGS__)
#define log_debug(...) log_msg(LOG_DEBUG, __VA_ARGS__)

#endif // LOGGER_H
