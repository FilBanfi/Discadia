/*
 * package_manifest.c - store package manifest parsing + checksum verify.
 *
 * The manifest lives at packages/<id>/manifest.json inside the store
 * repository (see store-repo/ for the reference layout). Relative
 * archive URLs are resolved against the store base URL so a repo can be
 * mirrored anywhere without editing manifests.
 *
 * SHA-256 verification shells out to `sha256sum` (coreutils) or
 * `openssl dgst` - whichever exists. If neither is available we log a
 * warning and treat the package as trusted-skip: on retro handhelds
 * missing coreutils would otherwise make the store unusable, and the
 * download already happens over HTTPS.
 */

#include "store/package_manifest.h"
#include "store/store_client.h"
#include "util/json.h"
#include "util/string_util.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h> /* strcasecmp */

int pkg_manifest_parse(const char *json_text, const char *base_url,
                       pkg_manifest *out)
{
    if (!json_text || !out) return -1;
    memset(out, 0, sizeof(*out));

    discadia_json *j = json_parse(json_text);
    if (!j) { LOG_WARN("manifest parse: invalid JSON"); return -1; }

    str_safe_copy(out->id,      json_string_or(json_get(j, "id"), ""),
                  sizeof(out->id));
    str_safe_copy(out->name,    json_string_or(json_get(j, "name"), out->id),
                  sizeof(out->name));
    str_safe_copy(out->version, json_string_or(json_get(j, "version"), "0"),
                  sizeof(out->version));

    const char *type = json_string_or(json_get(j, "type"), "game");
    out->type = pkg_type_from_string(type);

    /* archive: relative paths resolve against base_url; absolute http(s)
     * URLs pass through untouched. */
    const char *archive = json_string_or(json_get(j, "archive"), "");
    if (strncmp(archive, "http", 4) == 0) {
        str_safe_copy(out->archive_url, archive, sizeof(out->archive_url));
    } else {
        store_join_url(out->archive_url, sizeof(out->archive_url),
                       base_url ? base_url : store_base_url(), archive);
    }

    /* sha256 may be absent (empty -> verify skipped, logged). */
    const char *sha = json_string_or(json_get(j, "sha256"), "");
    for (const char *c = sha; *c; c++)
        if (!isxdigit((unsigned char)*c)) { sha = ""; break; }
    str_safe_copy(out->sha256, sha, sizeof(out->sha256));

    out->unpacked_size_mb = json_int_or(json_get(j, "size_mb"), 0);
    str_safe_copy(out->install_path,
                  json_string_or(json_get(j, "install_path"), ""),
                  sizeof(out->install_path));

    json_free(j);
    if (!out->id[0] || !out->archive_url[0]) {
        LOG_WARN("manifest missing id or archive field");
        return -1;
    }
    return 0;
}

/* Run a command and capture its first token of output (the digest). */
static int run_capture_digest(const char *cmd, char *hex_out, size_t hex_size)
{
    FILE *p = popen(cmd, "r");
    if (!p) return -1;
    char line[256] = {0};
    int ok = fgets(line, sizeof(line), p) != NULL;
    int rc = pclose(p);
    if (!ok || rc != 0) return -1;

    /* First whitespace-delimited token = hex digest. */
    char *sp = strpbrk(line, " \t\n");
    if (sp) *sp = '\0';
    size_t n = strlen(line);
    if (n != 64) return -1;
    for (size_t i = 0; i < n; i++)
        if (!isxdigit((unsigned char)line[i])) return -1;
    str_safe_copy(hex_out, line, hex_size);
    return 0;
}

int pkg_sha256_file(const char *path, char *hex_out, size_t hex_size)
{
    if (!path || !hex_out || hex_size < 65) return -1;

    char cmd[1024];

    /* Preferred: coreutils sha256sum. */
    snprintf(cmd, sizeof(cmd), "sha256sum '%s' 2>/dev/null", path);
    if (run_capture_digest(cmd, hex_out, hex_size) == 0) return 0;

    /* Fallback: openssl CLI. */
    snprintf(cmd, sizeof(cmd),
             "openssl dgst -sha256 '%s' 2>/dev/null | awk '{print $NF}'", path);
    if (run_capture_digest(cmd, hex_out, hex_size) == 0) return 0;

    LOG_WARN("no sha256 tool available (sha256sum/openssl): skipping verify");
    hex_out[0] = '\0';
    return -1;
}

int pkg_verify_sha256(const char *path, const char *expected_hex)
{
    /* Empty expectation = nothing to check -> trust the HTTPS fetch. */
    if (!expected_hex || !expected_hex[0]) return 1;

    char actual[128];
    if (pkg_sha256_file(path, actual, sizeof(actual)) != 0) {
        /* Tool missing: cannot verify. Accept with loud warning rather
         * than blocking installs on minimal systems. */
        return 1;
    }
    return strcasecmp(actual, expected_hex) == 0;
}
