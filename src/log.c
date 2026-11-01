#include "log.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static int g_debug_checked = 0;
static int g_debug_enabled = 0;

int discadia_debug_enabled(void) {
    if (!g_debug_checked) {
        const char *env = getenv("DISCADIA_DEBUG");
        g_debug_enabled = (env != NULL && strcmp(env, "0") != 0);
        g_debug_checked = 1;
    }
    return g_debug_enabled;
}

void discadia_log_write(const char *level, const char *fmt, ...) {
    time_t now = time(NULL);
    struct tm tm_buf;
    char stamp[64] = "-";

    if (localtime_r(&now, &tm_buf) != NULL) {
        strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tm_buf);
    }

    fprintf(stdout, "[%s] %s - ", level, stamp);

    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);

    fputc('\n', stdout);
    fflush(stdout);
}
