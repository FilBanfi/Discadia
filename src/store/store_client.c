/*
 * store_client.c - minimal HTTP fetch helper for the Discadia Store.
 *
 * Design constraints (see docs/ARCHITECTURE.md "Store"):
 *   - No libcurl dependency: we shell out to the system `curl` binary
 *     through a pipe. This keeps the launcher tiny and inherits whatever
 *     TLS backend the distro already ships.
 *   - Non-interactive: curl is always run with --silent/--show-error so
 *     it can never prompt on a kiosk device.
 *   - Bounded: every response is capped in bytes to protect against
 *     runaway or malicious servers.
 *
 * The base URL defaults to the raw endpoint of the official store repo
 * (https://github.com/filbanfi/discadia-store) and can be overridden at
 * runtime with the DISCADIA_STORE_URL environment variable, which also
 * makes local testing trivial (e.g. "file:///tmp/store" style mirrors
 * served over http).
 */

#include "store/store_client.h"
#include "log.h"
#include "util/string_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define STORE_DEFAULT_BASE "https://raw.githubusercontent.com/filbanfi/discadia-store/main"

char *store_http_get(const char *url, size_t max_bytes)
{
    if (!url || !*url || max_bytes == 0) return NULL;

    /* -f: fail silently on HTTP errors (non-zero exit), -L follow
     * redirects, --max-time prevents hangs on dead networks. */
    char cmd[SC_URL_LEN + 128];
    str_format(cmd, sizeof(cmd),
               "curl -fSL --silent --show-error --max-time 30 '%s' 2>/dev/null",
               url);

    FILE *p = popen(cmd, "r");
    if (!p) {
        LOG_WARN("store_http_get: popen failed (%s)", strerror(errno));
        return NULL;
    }

    /* Grow-as-you-read buffer; hard-capped at max_bytes. */
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) { pclose(p); return NULL; }

    for (;;) {
        if (len + 1024 > cap) {
            size_t ncap = cap * 2;
            if (ncap > max_bytes) ncap = max_bytes + 1; /* room for NUL */
            if (ncap <= cap) { /* already at the cap: truncate read */
                break;
            }
            char *nb = realloc(buf, ncap);
            if (!nb) { free(buf); pclose(p); return NULL; }
            buf = nb; cap = ncap;
        }
        size_t want = cap - len - 1;
        if (want > max_bytes - len) want = max_bytes - len;
        if (want == 0) break;
        size_t got = fread(buf + len, 1, want, p);
        len += got;
        if (got < want) break; /* EOF or error */
    }
    buf[len] = '\0';

    int rc = pclose(p);
    if (rc != 0) {
        /* curl exited non-zero: HTTP failure or connection problem. */
        LOG_WARN("store_http_get: curl failed for %s (rc=%d)", url, rc);
        free(buf);
        return NULL;
    }
    if (len == 0) { free(buf); return NULL; }
    return buf;
}

int store_http_download(const char *url, const char *dest_path,
                        long long max_bytes_hint)
{
    if (!url || !dest_path) return -1;

    /* Atomic download: write to <dest>.part then rename, so a crash or
     * power loss mid-install never leaves a half-written archive under
     * the final name. */
    char part[1024];
    str_format(part, sizeof(part), "%s.part", dest_path);

    char cmd[SC_URL_LEN + 1024];
    str_format(cmd, sizeof(cmd),
               "curl -fSL --silent --show-error --max-time 300 "
               "--output '%s' '%s' 2>/dev/null",
               part, url);

    LOG_INFO("store download: %s -> %s", url, dest_path);
    int rc = system(cmd);
    if (rc != 0) {
        LOG_WARN("store_http_download: curl rc=%d for %s", rc, url);
        remove(part);
        return -1;
    }

    /* Optional size sanity check (soft: log only, checksum decides). */
    if (max_bytes_hint > 0) {
        FILE *f = fopen(part, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long long sz = ftell(f);
            fclose(f);
            if (sz > max_bytes_hint * 4) /* generous 4x slack */
                LOG_WARN("download much larger than declared: %lld bytes", sz);
        }
    }

#ifdef _WIN32
    remove(dest_path);
#endif
    if (rename(part, dest_path) != 0) {
        LOG_ERROR("store_http_download: rename failed (%s)", strerror(errno));
        remove(part);
        return -1;
    }
    return 0;
}

void store_join_url(char *dst, size_t dst_size, const char *base,
                    const char *rel)
{
    if (!base || !*base) base = "";
    if (!rel) rel = "";
    while (*rel == '/') rel++;

    size_t bl = strlen(base);
    if (bl && base[bl - 1] == '/') {
        str_format(dst, dst_size, "%s%s", base, rel);
    } else {
        str_format(dst, dst_size, "%s/%s", base, rel);
    }
}

const char *store_base_url(void)
{
    static char cached[SC_URL_LEN];
    if (cached[0]) return cached;

    const char *env = getenv("DISCADIA_STORE_URL");
    if (env && *env) {
        str_safe_copy(cached, env, sizeof(cached));
        /* strip trailing slash for consistent joins */
        size_t n = strlen(cached);
        while (n > 0 && cached[n - 1] == '/') cached[--n] = '\0';
    } else {
        str_safe_copy(cached, STORE_DEFAULT_BASE, sizeof(cached));
    }
    return cached;
}
