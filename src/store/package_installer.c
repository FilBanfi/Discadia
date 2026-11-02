/*
 * package_installer.c - download, verify and extract store packages.
 *
 * Flow: fetch manifest -> check free space -> download archive into the
 * cache dir -> verify sha256 -> extract with tar (or copy for single
 * files like engines) -> write a .discadia-version marker -> cleanup.
 * User configuration (~/.config/discadia) is NEVER touched, so updates
 * can't overwrite settings.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "store/package_installer.h"
#include "store/store_client.h"
#include "util/fs.h"
#include "util/path_util.h"
#include "log.h"

const char *install_result_locale_key(install_result_t r)
{
    switch (r)
    {
        case INSTALL_ERR_NETWORK:  return "error_network";
        case INSTALL_ERR_CHECKSUM: return "error_checksum";
        case INSTALL_ERR_SPACE:    return "error_space";
        case INSTALL_ERR_EXTRACT:  return "error_extract";
        case INSTALL_ERR_PERMS:    return "error_perms";
        case INSTALL_ERR_MANIFEST: return "error_manifest";
        default:                   return "error_install_failed";
    }
}

static void user_data_dir(char *dst, size_t n)
{
    const char *home = getenv("HOME");
    if (!home || !*home) home = "/tmp";
    snprintf(dst, n, "%s/.local/share/discadia", home);
}

void installer_destination(pkg_type_t type, const char *id,
                           char *dst, size_t dst_size)
{
    char base[512];
    user_data_dir(base, sizeof(base));
    switch (type)
    {
        case PKG_THEME:  snprintf(dst, dst_size, "%s/themes/%s", base, id); break;
        case PKG_ENGINE: snprintf(dst, dst_size, "%s/engines", base);       break;
        default:         snprintf(dst, dst_size, "%s/games/%s", base, id);  break;
    }
}

int installer_is_installed(const store_pkg *pkg, char *cur_version,
                           size_t ver_size)
{
    if (cur_version && ver_size) cur_version[0] = '\0';
    if (!pkg) return 0;
    char dir[512], marker[640];
    installer_destination(pkg->type, pkg->id, dir, sizeof(dir));
    if (pkg->type == PKG_ENGINE)
    {
        snprintf(marker, sizeof(marker), "%s/%s.json", dir, pkg->id);
        if (!fs_file_exists(marker)) return 0;
    }
    else
    {
        snprintf(marker, sizeof(marker), "%s/.discadia-version", dir);
        if (!fs_file_exists(marker)) return 0;
    }
    if (cur_version && ver_size)
    {
        char *v = fs_read_file(marker, NULL);
        if (v)
        {
            /* strip trailing newline */
            size_t l = strlen(v);
            while (l && (v[l-1] == '\n' || v[l-1] == '\r')) v[--l] = '\0';
            snprintf(cur_version, ver_size, "%s", v);
            free(v);
        }
    }
    return 1;
}

int installer_fetch_manifest(const store_pkg *pkg, const char *base_url,
                             pkg_manifest *out)
{
    if (!pkg || !out) return -1;
    char url[SC_URL_LEN];
    store_join_url(url, sizeof(url), base_url, pkg->manifest_url);
    char *text = store_http_get(url, 256 * 1024);
    if (!text) return -1;
    int rc = pkg_manifest_parse(text, base_url, out);
    free(text);
    return rc;
}

/* Single-file install (engine JSON): archive is the file itself. */
static int install_single_file(const char *archive, const char *dst_dir,
                               const char *id)
{
    if (fs_mkdir_p(dst_dir, 0755) != 0) return -1;
    char dst[768];
    snprintf(dst, sizeof(dst), "%s/%s.json", dst_dir, id);
    char *data = fs_read_file(archive, NULL);
    if (!data) return -1;
    int rc = fs_write_file_atomic(dst, data, strlen(data));
    free(data);
    return rc;
}

install_result_t installer_install(const store_pkg *pkg,
                                   const char *base_url,
                                   const char *cache_dir,
                                   char *out_detail, size_t detail_size)
{
    if (out_detail && detail_size) out_detail[0] = '\0';
    if (!pkg || !cache_dir) return INSTALL_ERR_MANIFEST;

    pkg_manifest mf;
    if (installer_fetch_manifest(pkg, base_url, &mf) != 0)
        return INSTALL_ERR_MANIFEST;

    char dst_dir[512];
    if (mf.install_path[0])
        snprintf(dst_dir, sizeof(dst_dir), "%s", mf.install_path);
    else
        installer_destination(pkg->type, pkg->id, dst_dir, sizeof(dst_dir));

    /* Free-space guard: unpacked size + 32 MB headroom. */
    long long need = (long long)(mf.unpacked_size_mb ? mf.unpacked_size_mb
                                 : pkg->size_mb + 16) * 1024 * 1024;
    long long have = fs_free_space_bytes("/");
    char probe[512];
    path_dirname(probe, sizeof(probe), dst_dir);
    long long have2 = fs_free_space_bytes(probe[0] ? probe : "/");
    if (have >= 0 && have2 >= 0) have = have < have2 ? have : have2;
    if (have >= 0 && have < need + 32LL * 1024 * 1024)
        return INSTALL_ERR_SPACE;

    /* Download archive to cache. */
    char archive[832];
    const char *fname = strrchr(mf.archive_url, '/');
    snprintf(archive, sizeof(archive), "%s/%s", cache_dir,
             (fname && fname[1]) ? fname + 1 : "package.download");
    if (store_http_download(mf.archive_url, archive,
                            (pkg->size_mb > 0 ? pkg->size_mb : 512) *
                            1024 * 1024) != 0)
        return INSTALL_ERR_NETWORK;

    /* Verify checksum (skips gracefully when no tooling/no digest). */
    if (!pkg_verify_sha256(archive, mf.sha256))
    {
        fs_remove_file(archive);
        return INSTALL_ERR_CHECKSUM;
    }

    /* Extract or copy. */
    int rc;
    if (pkg->type == PKG_ENGINE)
        rc = install_single_file(archive, dst_dir, pkg->id);
    else
    {
        if (fs_mkdir_p(dst_dir, 0755) != 0) rc = -1;
        else
        {
            char cmd[1600];
            snprintf(cmd, sizeof(cmd), "tar -xf '%s' -C '%s'", archive, dst_dir);
            rc = (system(cmd) == 0) ? 0 : -1;
        }
    }
    fs_remove_file(archive);

    if (rc != 0)
    {
        if (access(dst_dir, W_OK) != 0) return INSTALL_ERR_PERMS;
        return INSTALL_ERR_EXTRACT;
    }

    /* Version marker (never inside ~/.config -> settings are safe). */
    char marker[640];
    if (pkg->type != PKG_ENGINE)
    {
        snprintf(marker, sizeof(marker), "%s/.discadia-version", dst_dir);
        fs_write_file_atomic(marker, mf.version, strlen(mf.version));
    }
    LOG_I("store: installed %s v%s -> %s", pkg->id, mf.version, dst_dir);
    return INSTALL_OK;
}
