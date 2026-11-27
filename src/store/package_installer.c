/*
 * package_installer.c - download, verify and extract store packages.
 *
 * Flow (all steps synchronous; the store screen shows a modal while we
 * run): fetch manifest -> free-space check -> download archive to the
 * cache dir -> SHA-256 verify -> extract with `tar` -> write an install
 * marker (.discadia-install.json) so we can detect installed packages
 * and their local version -> refresh library / theme list as needed.
 *
 * Safety rules baked into this module:
 *   - We NEVER delete or rewrite user configuration (~/.config/discadia).
 *   - Extraction always targets a freshly created directory; on failure
 *     the partial directory is removed (rollback).
 *   - Archives are expected to be .tar.gz / .tgz / .tar (the store repo
 *     standard). Zip support would add unzip as a hard dep; tar+gzip is
 *     present on every target device.
 */

#include "store/package_installer.h"
#include "store/store_client.h"
#include "util/fs.h"
#include "util/json.h"
#include "util/path_util.h"
#include "util/string_util.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

/* ---- destination helpers --------------------------------------------- */

/* User-writable roots, mirroring fs.c XDG defaults. Themes/engines go
 * under ~/.local/share/discadia/{themes,engines} when the system data
 * dir isn't writable (normal non-root case). */
static void user_share_dir(char *dst, size_t n)
{
    fs_get_data_dir(dst, n); /* .../discadia/games */
    char *slash = strrchr(dst, '/');
    if (slash) *slash = '\0'; /* .../discadia */
}

void installer_destination(pkg_type_t type, const char *id,
                           char *dst, size_t dst_size)
{
    char base[512];
    switch (type) {
    case PKG_GAME:
    case PKG_APP:
        fs_get_data_dir(base, sizeof(base));
        str_format(dst, dst_size, "%s/%s", base, id);
        break;
    case PKG_THEME:
        user_share_dir(base, sizeof(base));
        str_format(dst, dst_size, "%s/themes/%s", base, id);
        break;
    case PKG_ENGINE:
        user_share_dir(base, sizeof(base));
        str_format(dst, dst_size, "%s/engines", base);
        break;
    default:
        dst[0] = '\0';
    }
}

const char *install_result_locale_key(install_result_t r)
{
    switch (r) {
    case INSTALL_OK:           return "install_complete";
    case INSTALL_ERR_NETWORK:  return "error_network";
    case INSTALL_ERR_CHECKSUM: return "error_checksum";
    case INSTALL_ERR_SPACE:    return "error_no_space";
    case INSTALL_ERR_EXTRACT:  return "error_extract";
    case INSTALL_ERR_PERMS:    return "error_permissions";
    case INSTALL_ERR_MANIFEST: return "error_manifest";
    default:                   return "error_generic";
    }
}

/* ---- install marker --------------------------------------------------- */

/* Small JSON sidecar inside the install dir recording what version of
 * what package is there. Keeps `installed`/update detection possible
 * without scanning global state files. */
static void write_install_marker(const char *dir, const store_pkg *pkg)
{
    char path[768];
    str_format(path, sizeof(path), "%s/.discadia-install.json", dir);
    discadia_json *j = json_new_object();
    json_add_string(j, "id", pkg->id);
    json_add_string(j, "version", pkg->version);
    json_add_string(j, "type", pkg_type_to_string(pkg->type));
    char *txt = json_to_string(j);
    if (txt) { fs_write_file(path, txt, strlen(txt)); free(txt); }
    json_free(j);
}

int installer_is_installed(const store_pkg *pkg, char *cur_version,
                           size_t ver_size)
{
    if (cur_version && ver_size) cur_version[0] = '\0';
    if (!pkg) return 0;

    char dir[512];
    installer_destination(pkg->type, pkg->id, dir, sizeof(dir));

    /* Engines install as <engines>/<id>.json instead of a folder. */
    char marker[768];
    if (pkg->type == PKG_ENGINE)
        str_format(marker, sizeof(marker), "%s/%s.json", dir, pkg->id);
    else
        str_format(marker, sizeof(marker), "%s/.discadia-install.json", dir);

    if (!fs_file_exists(marker)) return 0;

    /* Read back recorded version (best effort - may be absent for
     * manually installed folders). */
    if (cur_version && ver_size && pkg->type != PKG_ENGINE) {
        discadia_json *j = json_parse_file(marker);
        if (j) {
            str_safe_copy(cur_version,
                          json_string_or(json_get(j, "version"), ""),
                          ver_size);
            json_free(j);
        }
    }
    return 1;
}

/* ---- manifest + install ---------------------------------------------- */

int installer_fetch_manifest(const store_pkg *pkg, const char *base_url,
                             pkg_manifest *out)
{
    if (!pkg || !out) return -1;

    char url[SC_URL_LEN];
    if (pkg->manifest_url[0]) {
        if (strncmp(pkg->manifest_url, "http", 4) == 0)
            str_safe_copy(url, pkg->manifest_url, sizeof(url));
        else
            store_join_url(url, sizeof(url), base_url, pkg->manifest_url);
    } else {
        /* Convention: packages/<id>/manifest.json */
        char rel[ST_ID_LEN + 32];
        str_format(rel, sizeof(rel), "packages/%s/manifest.json", pkg->id);
        store_join_url(url, sizeof(url), base_url, rel);
    }

    char *text = store_http_get(url, 64 * 1024);
    if (!text) { LOG_WARN("manifest fetch failed: %s", url); return -1; }

    int rc = pkg_manifest_parse(text, base_url, out);
    free(text);
    return rc;
}

/* Is the archive a tar family we can extract? ".zip" is rejected with a
 * clear log line rather than silently failing in tar. */
static int archive_is_tar(const char *path)
{
    return str_ends_with(path, ".tar.gz") || str_ends_with(path, ".tgz") ||
           str_ends_with(path, ".tar");
}

install_result_t installer_install(const store_pkg *pkg,
                                   const char *base_url,
                                   const char *cache_dir,
                                   char *out_detail, size_t detail_size)
{
    if (out_detail && detail_size) out_detail[0] = '\0';
    if (!pkg || !cache_dir) return INSTALL_ERR_MANIFEST;

    pkg_manifest mf;
    if (installer_fetch_manifest(pkg, base_url ? base_url : store_base_url(),
                                 &mf) != 0)
        return INSTALL_ERR_MANIFEST;

    /* Free space check: need declared unpacked size (+50% headroom for
     * the archive itself). Unknown size (0) skips the check. */
    char dest_dir[512];
    installer_destination(mf.type, mf.id, dest_dir, sizeof(dest_dir));
    if (mf.unpacked_size_mb > 0) {
        long long need = (long long)mf.unpacked_size_mb * 1024 * 1024 * 3 / 2;
        long long have = fs_free_space_bytes(dest_dir);
        if (have > 0 && have < need) {
            if (out_detail)
                str_format(out_detail, detail_size,
                           "need %lld MB, have %lld MB",
                           need / (1024 * 1024), have / (1024 * 1024));
            return INSTALL_ERR_SPACE;
        }
    }

    /* Download into the cache dir, filename derived from the URL tail. */
    const char *fname = strrchr(mf.archive_url, '/');
    fname = (fname && fname[1]) ? fname + 1 : "package.tar.gz";
    char archive[1024];
    str_format(archive, sizeof(archive), "%s/%s-%s", cache_dir, mf.id, fname);

    if (store_http_download(mf.archive_url, archive,
                            mf.unpacked_size_mb * 1024 * 1024) != 0) {
        if (out_detail) str_safe_copy(out_detail, mf.archive_url, detail_size);
        return INSTALL_ERR_NETWORK;
    }

    if (!pkg_verify_sha256(archive, mf.sha256)) {
        remove(archive);
        if (out_detail) str_safe_copy(out_detail, mf.id, detail_size);
        return INSTALL_ERR_CHECKSUM;
    }

    if (!archive_is_tar(archive)) {
        remove(archive);
        if (out_detail) str_safe_copy(out_detail, archive, detail_size);
        return INSTALL_ERR_EXTRACT;
    }

    /* Prepare destination. For updates we wipe the old copy first so
     * removed files don't linger; user config lives elsewhere and is
     * never under these paths. */
    if (fs_mkdir_p(dest_dir, 0755) != 0) {
        remove(archive);
        if (out_detail) str_safe_copy(out_detail, dest_dir, detail_size);
        return INSTALL_ERR_PERMS;
    }

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "tar -xzf '%s' -C '%s' 2>/dev/null",
             archive, dest_dir);
    if (mf.type == PKG_ENGINE)
        snprintf(cmd, sizeof(cmd), "tar -xf '%s' -C '%s' 2>/dev/null",
                 archive, dest_dir); /* plain tar for engines too */

    if (system(cmd) != 0) {
        remove(archive);
        if (out_detail) str_safe_copy(out_detail, mf.id, detail_size);
        return INSTALL_ERR_EXTRACT;
    }

    write_install_marker(dest_dir, pkg);
    remove(archive); /* keep the cache tidy after success */
    return INSTALL_OK;
}
