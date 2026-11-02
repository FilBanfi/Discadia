/*
 * package_manifest.c - manifest.json parsing + SHA-256 verification.
 *
 * Checksums are computed with `sha256sum` (coreutils, present on every
 * target) or `openssl dgst` as fallback. When neither tool exists the
 * file is accepted with a loud warning rather than blocking installs -
 * per spec, missing tooling must never hard-fail the launcher.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "store/package_manifest.h"
#include "store/store_client.h"
#include "util/json.h"
#include "util/fs.h"
#include "log.h"

int pkg_manifest_parse(const char *json_text, const char *base_url,
                       pkg_manifest *out)
{
    if (!json_text || !out) return -1;
    memset(out, 0, sizeof(*out));

    discadia_json *root = json_parse(json_text);
    if (!root) return -1;

    const char *id = json_get_string_or(root, "id", "");
    snprintf(out->id, sizeof(out->id), "%s", id);
    const char *nm = json_get_string_or(root, "name", id);
    snprintf(out->name, sizeof(out->name), "%s", nm);
    const char *ver = json_get_string_or(root, "version", "");
    snprintf(out->version, sizeof(out->version), "%s", ver);
    out->type = pkg_type_from_string(json_get_string_or(root, "type", "game"));

    /* archive: absolute URL (own GitHub release) or repo-relative. */
    char url[768] = "";
    const char *au = json_get_string_or(root, "archive_url", NULL);
    if (!au)
    {
        const char *rel = json_get_string_or(root, "archive", NULL);
        if (rel && id[0])
        {
            char tmp[512];
            snprintf(tmp, sizeof(tmp), "packages/%s/%s", id, rel);
            store_join_url(url, sizeof(url), base_url, tmp);
        }
    }
    else
        store_join_url(url, sizeof(url), base_url, au);
    snprintf(out->archive_url, sizeof(out->archive_url), "%s", url);

    const char *sum = json_get_string_or(root, "sha256", "");
    snprintf(out->sha256, sizeof(out->sha256), "%s", sum);
    out->unpacked_size_mb = json_get_int_or(root, "unpacked_size_mb", 0);
    const char *ip = json_get_string_or(root, "install_path", "");
    snprintf(out->install_path, sizeof(out->install_path), "%s", ip);

    json_free(root);
    return out->archive_url[0] ? 0 : -1;   /* no archive -> useless pkg */
}

int pkg_sha256_file(const char *path, char *hex_out, size_t hex_size)
{
    if (!path || !hex_out || hex_size < 65) return -1;

    FILE *p;
    char cmd[1200];
    snprintf(cmd, sizeof(cmd), "sha256sum '%s' 2>/dev/null", path);
    p = popen(cmd, "r");
    if (!p) return -1;
    char line[256] = "";
    size_t got = fread(line, 1, sizeof(line) - 1, p);
    line[got] = '\0';
    int rc = pclose(p);

    if (rc == 0 && got >= 64)
    {
        memcpy(hex_out, line, 64);
        hex_out[64] = '\0';
        return 0;
    }
    /* openssl fallback (some minimal images lack coreutils). */
    snprintf(cmd, sizeof(cmd),
             "openssl dgst -sha256 '%s' 2>/dev/null", path);
    p = popen(cmd, "r");
    if (!p) return -1;
    got = fread(line, 1, sizeof(line) - 1, p);
    line[got] = '\0';
    rc = pclose(p);
    char *sep = strstr(line, "= ");
    if (rc == 0 && sep && strlen(sep) >= 66)
    {
        memcpy(hex_out, sep + 2, 64);
        hex_out[64] = '\0';
        return 0;
    }
    return -1;
}

int pkg_verify_sha256(const char *path, const char *expected_hex)
{
    if (!expected_hex || !*expected_hex) return 1;   /* nothing to check */
    char actual[80];
    if (pkg_sha256_file(path, actual, sizeof(actual)) != 0)
    {
        LOG_W("checksum: no sha256 tool available, trusting %s", path);
        return 1;    /* graceful: never block install for missing tooling */
    }
    return strcasecmp(actual, expected_hex) == 0 ? 1 : 0;
}
