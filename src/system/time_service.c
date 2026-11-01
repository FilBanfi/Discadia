/*
 * time_service.c - see time_service.h
 */

#include "time_service.h"
#include <stdio.h>
#include <time.h>

void time_now_string(char *dst, int dst_size, ts_format_t fmt)
{
    if (!dst || dst_size <= 0) return;
    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    if (fmt == TS_12H)
    {
        int h = lt.tm_hour % 12;
        if (h == 0) h = 12;
        snprintf(dst, dst_size, "%d:%02d %s", h, lt.tm_min,
                 lt.tm_hour < 12 ? "AM" : "PM");
    }
    else
    {
        snprintf(dst, dst_size, "%02d:%02d", lt.tm_hour, lt.tm_min);
    }
}

void time_date_string(char *dst, int dst_size)
{
    if (!dst || dst_size <= 0) return;
    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    snprintf(dst, dst_size, "%04d-%02d-%02d", lt.tm_year + 1900,
             lt.tm_mon + 1, lt.tm_mday);
}
