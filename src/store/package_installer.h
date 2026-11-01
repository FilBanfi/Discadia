#ifndef DISCADIA_PACKAGE_INSTALLER_H
#define DISCADIA_PACKAGE_INSTALLER_H

/*
 * Package installer: downloads the archive into the cache dir, verifies
 * the checksum, extracts it to the correct destination per type and
 * refreshes the library. User configuration is NEVER touched: installs
 * only write under data folders, never into ~/.config/discadia.
 *
 * Destinations:
 *   game   -> ~/.local/share/discadia/games/<id>/
 *   theme  -> <data_dir>/themes/<id>/        (falls back to user themes)
 *   engine -> <data_dir>/engines/<id>.json
 *   app    -> ~/.local/share/discadia/games/<id>/
 *
 * Themes/engines are installed to the user-writable directory
 * ~/.local/share/discadia/{themes,engines} when the system data dir is
 * not writable, keeping non-root usage fully functional.
 */

#include "store/store_model.h"
#include "store/package_manifest.h"

typedef enum install_result {
    INSTALL_OK = 0,
    INSTALL_ERR_NETWORK,        /* download failed                      */
    INSTALL_ERR_CHECKSUM,       /* sha256 mismatch                      */
    INSTALL_ERR_SPACE,          /* not enough free space                */
    INSTALL_ERR_EXTRACT,        /* tar failed                           */
    INSTALL_ERR_PERMS,          /* cannot write destination             */
    INSTALL_ERR_MANIFEST        /* manifest missing/unparsable          */
} install_result_t;

/* Human-readable reason for a failure (i18n key). */
const char *install_result_locale_key(install_result_t r);

/* Fetch + parse the manifest of `pkg`. Returns 0 on success. */
int installer_fetch_manifest(const store_pkg *pkg, const char *base_url,
                             pkg_manifest *out);

/* Full install (manifest fetch, download, verify, extract). Blocking -
 * called from the store screen which shows a modal "Installing..." state.
 * Returns INSTALL_OK or an error code with detail in out_detail. */
install_result_t installer_install(const store_pkg *pkg,
                                   const char *base_url,
                                   const char *cache_dir,
                                   char *out_detail, size_t detail_size);

/* Where would this package be installed? Writes absolute path. */
void installer_destination(pkg_type_t type, const char *id,
                           char *dst, size_t dst_size);

/* Is a package already installed locally? Reads its stored version into
 * cur_version when available (may be empty). Returns 0/1. */
int installer_is_installed(const store_pkg *pkg, char *cur_version,
                           size_t ver_size);

#endif /* DISCADIA_PACKAGE_INSTALLER_H */
