#include "logger.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>

static log_level_t g_level=LOG_INFO;
static char g_prefix[32];  // ex [DHCPv6]
static int g_fd = -1;       /* file descriptor for file logging */
static int g_use_file = 0; /* 1 -> use g_fd; 0 -> stdout/stderr */
static int g_inited = 0;   /* 1 -> init_logger() succeeded at least once */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
//Internal helpers (minimal and allocation-free)

/* Choose output FD (file, stderr for WARN/ERROR, stdout for INFO/DEBUG) */
static int pick_fd_for_level(log_level_t level)
{
    if(g_use_file && g_fd >= 0) return g_fd;

    if(level == LOG_ERROR || level == LOG_WARN)
    {
        return 2;
    }
    else
    {
        return 1;
    }
}

/* Build UTC timestamp "YYYY-MM-DD HH:MM:SS" */
static void build_timestamp(char *dst, size_t dst_sz)
{
    time_t now = time(NULL);
    struct tm tmbuf;
    struct tm *tm = localtime_r(&now, &tmbuf);
    if (!tm) {
        snprintf(dst, dst_sz, "0000-00-00 00:00:00");
        return;
    }

    strftime(dst, dst_sz, "%Y-%m-%d %H:%M:%S", tm);
}


static void buff_append_format(char *buf, int buf_sz, int *len, const char *fmt, va_list ap)
{

    if (*len >= buf_sz) return;
    int written = vsnprintf(buf + *len, buf_sz - *len, fmt, ap);
    if (written < 0) {
        /* Ignore formatting errors, keep what we have. */
        return;
    }
    if (written >= (buf_sz - *len)) {
        /* Truncated */
        *len = buf_sz - 1;
    } else {
        *len += written;
    }
}                                                                                                   


log_level_t log_get_level(){
    pthread_mutex_lock(&g_lock);
    log_level_t lvl = g_level;
    pthread_mutex_unlock(&g_lock);
    return lvl;
}
void log_set_level(log_level_t level)
{
    pthread_mutex_lock(&g_lock);
    g_level = level;
    pthread_mutex_unlock(&g_lock);
}

int init_logger(const char *prefix, log_level_t level, bool to_file, const char *path)
{
    pthread_mutex_lock(&g_lock);
    /* Close previous file if re-initializing to a different target */
   if(g_use_file && g_fd >= 0)
   {
    close(g_fd );
    g_fd = -1;
   }

   g_inited = 0;
   g_use_file=0;
   g_level = level;

    /* Copy prefix safely */
   g_prefix[0]='\0';
   if(prefix && prefix[0]!='\0')
   {
        size_t i = 0;
        for(; prefix[i] && i<sizeof(g_prefix)-1;++i)
        {
            g_prefix[i]=prefix[i];
        }
        g_prefix[i]='\0';
   }

   if(to_file)
   {
    if(!path || path[0]=='\0')
    {
        pthread_mutex_unlock(&g_lock);
        return -1;
    }

     int flags = O_WRONLY | O_CREAT | O_APPEND;
     #ifdef O_CLOEXEC
     flags |= O_CLOEXEC;
     #endif

     int fd = open(path,flags,0644);
     if(fd < 0)
     {
        pthread_mutex_unlock(&g_lock);
        return -1;
     }


     g_fd=fd;
     g_use_file=1;

   }
   else
   {
    g_use_file=0;
    g_fd=-1;
   }

   g_inited = 1;
   pthread_mutex_unlock(&g_lock);
   return 0;
}

void log_msg(log_level_t level, const char *format, ...)
{
    pthread_mutex_lock(&g_lock);

    if (!g_inited)
        (void)init_logger("[UNINITIALIZED]", LOG_INFO, false, NULL);

    if (level < g_level) {
        pthread_mutex_unlock(&g_lock);
        return;
    }

    static const char *LEVEL_STR[] = { "DEBUG", "INFO", "WARN", "ERROR" };

    char line[1024];
    char ts[32];
    build_timestamp(ts, sizeof(ts));

    int len = snprintf(line, sizeof(line),
                       "%s %s [%s] ",
                       ts,
                       (g_prefix[0] ? g_prefix : ""),
                       LEVEL_STR[level]);

    va_list ap;
    va_start(ap, format);
    vsnprintf(line + len, sizeof(line) - len, format, ap);
    va_end(ap);

    strncat(line, "\n", sizeof(line) - strlen(line) - 1);

    int fd = pick_fd_for_level(level);
    if (fd >= 0) write(fd, line, strlen(line));

    pthread_mutex_unlock(&g_lock);
}


/* Close and reset logger state */
void close_logger()
{
    pthread_mutex_lock(&g_lock);
    if(g_use_file && g_fd >= 0)
    {
        close(g_fd);
    }

    g_fd = -1;
    g_inited = 0;
    g_use_file = 0;
    pthread_mutex_unlock(&g_lock);
}