#ifndef DISCADIA_STORE_CLIENT_H
#define DISCADIA_STORE_CLIENT_H

/*
 * Store HTTP client. Uses the system `curl` binary through a pipe -
 * no libcurl dependency, keeps the binary lean and inherits whatever
 * TLS backend the distro provides. Downloads are capped in size and
 * always non-interactive (never prompts).
 */

#include <stddef.h>

#define SC_URL_LEN 768

/* Fetch a URL into a malloc'd buffer (NULL on failure). max_bytes caps
 * the download to protect against runaway responses. Caller frees. */
char *store_http_get(const char *url, size_t max_bytes);

/* Download url to a local file (atomic: .part then rename). Returns 0. */
int store_http_download(const char *url, const char *dest_path,
                        long long max_bytes_hint);

/* Build a full URL from base + relative path, handling slashes. */
void store_join_url(char *dst, size_t dst_size, const char *base,
                    const char *rel);

/* Default store base URL: env DISCADIA_STORE_URL, else the upstream
 * GitHub raw endpoint of the router repository. Static buffer. */
const char *store_base_url(void);

#endif /* DISCADIA_STORE_CLIENT_H */
