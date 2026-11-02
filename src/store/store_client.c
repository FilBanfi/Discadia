/*
 * store_client.c - HTTP through the system `curl` binary (popen pipes).
 * No libcurl dependency: keeps the binary tiny and inherits whatever
 * TLS backend the distro already ships. Everything is non-interactive
 * and size-capped so a runaway server can never hang or OOM us.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "store/store_client.h"
#include "util/fs.h"
#include "log.h"

#define CURL_COMMON \
    "curl -sSL --fail --max-time 30 --connect-timeout 8 " \
    "--speed-limit 1024 --speed-time 20 --proto https,http " \
    "--user-agent 'discadia/" "1.0" "'"

char *store_http_get(const char *url, size_t max_bytes)
{
    if (!url || !*url || max_bytes == 0) return NULL;

    /* Quote the URL to keep the shell from interpreting anything. */
    char cmd[SC_URL_LEN + 256];
    snprintf(cmd, sizeof(cmd), "%s -r 0-%zu '%s' 2>/dev/null",
             CURL_COMMON, max_bytes - 1, url);

    FILE *p = popen(cmd, "r");
    if (!p) { LOG_W("store: popen(curl) failed"); return NULL; }

    size_t cap = 64 * 1024, len = 0;
    if (cap > max_bytes) cap = max_bytes;
    char *buf = malloc(cap);
    if (!buf) { pclose(p); return NULL; }

    size_t n;
    while (len < cap && (n = fread(buf + len, 1, cap - len, p)) > 0)
        len += n;
    int rc = pclose(p);

    if (rc != 0 || len == 0)
    {
        LOG_W("store: GET failed for %s (rc=%d)", url, rc);
        free(buf);
        return NULL;
    }
    /* Grow only if the cap was not reached; then NUL-terminate. */
    if (len < cap)
    {
        char *nb = realloc(buf, len + 1);
        if (nb) buf = nb;
    }
    else
    {
        char *nb = realloc(buf, cap + 1);
        if (!nb) { free(buf); return NULL; }
        buf = nb;
    }
    buf[len] = '\0';
    return buf;
}

int store_http_download(const char *url, const char *dest_path,
                        long long max_bytes_hint)
{
    if (!url || !dest_path) return -1;

    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s.part", dest_path);
    fs_remove_file(tmp);   /* stale partial from a previous crash */

    char limit[64] = "";
    if (max_bytes_hint > 0)
        snprintf(limit, sizeof(limit), "--max-filesize %lld ", max_bytes_hint);

    char cmd[SC_URL_LEN + 512];
    snprintf(cmd, sizeof(cmd),
             "%s %s-o '%s' '%s' 2>/dev/null",
             CURL_COMMON, limit, tmp, url);

    int rc = system(cmd);
    if (rc != 0)
    {
        LOG_W("store: download failed for %s (rc=%d)", url, rc);
        fs_remove_file(tmp);
        return -1;
    }
    /* Atomic-ish: only publish the file once fully downloaded. */
    if (rename(tmp, dest_path) != 0)
    {
        LOG_W("store: cannot move %s -> %s", tmp, dest_path);
        fs_remove_file(tmp);
        return -1;
    }
    return 0;
}

void store_join_url(char *dst, size_t dst_size, const char *base,
                    const char *rel)
{
    if (!dst || dst_size == 0) return;
    if (!rel || !*rel) { snprintf(dst, dst_size, "%s", base ? base : ""); return; }
    /* Absolute rel wins (packages may point at their own GitHub release). */
    if (!strncmp(rel, "http://", 7) || !strncmp(rel, "https://", 8))
    {
        snprintf(dst, dst_size, "%s", rel);
        return;
    }
    if (!base || !*base) { snprintf(dst, dst_size, "%s", rel); return; }
    size_t bl = strlen(base);
    int slash = (bl && base[bl - 1] == '/');
    int lead  = (rel[0] == '/');
    if (slash && lead)
        snprintf(dst, dst_size, "%s%s", base, rel + 1);
    else if (!slash && !lead)
        snprintf(dst, dst_size, "%s/%s", base, rel);
    else
        snprintf(dst, dst_size, "%s%s", base, rel);
}

const char *store_base_url(void)
{
    static char cached[SC_URL_LEN] = {0};
    if (cached[0]) return cached;

    const char *env = getenv("DISCADIA_STORE_URL");
    if (env && *env)
    {
        snprintf(cached, sizeof(cached), "%s", env);
        return cached;
    }
    /* Router repository raw endpoint (index.json lives here; the actual
     * package payloads are hosted in each project's own GitHub repo and
     * referenced by absolute URLs inside their manifest.json). */
    snprintf(cached, sizeof(cached),
             "https://raw.githubusercontent.com/discadia/discadia-store/main");
    return cached;
}
