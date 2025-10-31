#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include "../include/time_utils.h"

time_t parse_lease_time(const char* time_str)
{
    if(!time_str) return 0;

    struct tm tm_info = {0};
    int day_of_week;

    // Parse: "4 2024/10/26 14:30:00"
    int parsed = sscanf(time_str, "%d %d/%d/%d %d:%d:%d",
                       &day_of_week,
                       &tm_info.tm_year,
                       &tm_info.tm_mon,
                       &tm_info.tm_mday,
                       &tm_info.tm_hour,
                       &tm_info.tm_min,
                       &tm_info.tm_sec);

    if(parsed != 7)
    {
        // Fallback: try parsing as Unix timestamp
        return (time_t)atoll(time_str);
    }

    // Adjust values for struct tm
    tm_info.tm_year -= 1900;  // Years since 1900
    tm_info.tm_mon -= 1;       // Months since January (0-11)
    tm_info.tm_isdst = -1;     // Auto-detect DST

    return mktime(&tm_info);
}

void format_lease_time(time_t timestamp, char* output, size_t output_len)
{
    if(!output || output_len == 0) return;

    struct tm* tm_info = localtime(&timestamp);
    if(!tm_info)
    {
        snprintf(output, output_len, "0");
        return;
    }

    int day_of_week = tm_info->tm_wday;  // 0=Sunday, 1=Monday, ..., 6=Saturday

    snprintf(output, output_len, "%d %04d/%02d/%02d %02d:%02d:%02d",
             day_of_week,
             tm_info->tm_year + 1900,
             tm_info->tm_mon + 1,
             tm_info->tm_mday,
             tm_info->tm_hour,
             tm_info->tm_min,
             tm_info->tm_sec);
}
