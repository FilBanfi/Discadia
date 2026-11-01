/*
 * network_status.c - see network_status.h
 */

#include "network_status.h"
#include "util/fs.h"
#include "util/string_util.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

static int s_cached = -1;
static time_t s_cached_at = 0;

int network_is_online(void)
{
    time_t now = time(NULL);
    if (s_cached >= 0 && now - s_cached_at < 2) return s_cached;
    s_cached_at = now;

    int online = 0;
    DIR *d = opendir("/sys/class/net");
    if (!d) { s_cached = 0; return 0; }
    struct dirent *e;
    while ((e = readdir(d)) != NULL)
    {
        if (e->d_name[0] == '.' || strcmp(e->d_name, "lo") == 0) continue;
        char path[300], val[8];
        str_format(path, sizeof(path), "/sys/class/net/%s/carrier", e->d_name);
        char *c = fs_read_file(path, NULL);
        if (c)
        {
            str_safe_copy(val, c, sizeof(val));
            free(c);
            if (val[0] == '1') { online = 1; break; }
        }
    }
    closedir(d);
    s_cached = online;
    return online;
}
