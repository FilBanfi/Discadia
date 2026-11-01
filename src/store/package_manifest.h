#ifndef DISCADIA_PACKAGE_MANIFEST_H
#define DISCADIA_PACKAGE_MANIFEST_H

/*
 * Package manifest parsing (packages/<id>/manifest.json in the store
 * repo) plus SHA-256 checksum verification of downloaded archives.
 */

#include "store/store_model.h"

typedef struct pkg_manifest {
    char id[ST_ID_LEN];
    char name[ST_NAME_LEN];
    char version[32];
    pkg_type_t type;
    char archive_url[768];      /* resolved absolute URL                */
    char sha256[128];           /* hex digest or empty when absent      */
    long unpacked_size_mb;
    /* Destination override; empty -> default per type. */
    char install_path[512];
} pkg_manifest;

/* Parse manifest JSON text; `base_url` resolves relative archive paths.
 * Returns 0 on success. */
int pkg_manifest_parse(const char *json_text, const char *base_url,
                       pkg_manifest *out);

/* Compute the SHA-256 hex digest of a file. Returns 0 on success.
 * Implemented with openssl if present, else falls back to accepting
 * any checksum (logged warning) so installs never hard-fail because a
 * tool is missing. */
int pkg_sha256_file(const char *path, char *hex_out, size_t hex_size);

/* Verify file matches expected digest (empty expected = skip check).
 * Returns 1 ok / trusted-skip, 0 mismatch/unverifiable. */
int pkg_verify_sha256(const char *path, const char *expected_hex);

#endif /* DISCADIA_PACKAGE_MANIFEST_H */
