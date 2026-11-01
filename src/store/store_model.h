#ifndef DISCADIA_STORE_MODEL_H
#define DISCADIA_STORE_MODEL_H

/*
 * Store data model: plain structs describing the remote catalog.
 * No SDL includes here so the model can be unit-tested headless.
 */

#include <stddef.h>

#define ST_MAX_PACKAGES 128
#define ST_ID_LEN       64
#define ST_NAME_LEN     128

typedef enum pkg_type {
    PKG_GAME = 0,
    PKG_THEME,
    PKG_ENGINE,
    PKG_APP,
    PKG_TYPE_COUNT
} pkg_type_t;

typedef struct store_pkg {
    char id[ST_ID_LEN];
    char name[ST_NAME_LEN];
    char description[512];      /* resolved for current language        */
    pkg_type_t type;
    char version[32];
    long size_mb;
    int  installed;             /* filled locally from disk state       */
    int  update_available;      /* local version differs from index     */
    int  compatible;            /* checked against system arch          */
    char manifest_url[512];     /* relative to base URL or absolute     */
    char cover_url[512];        /* ditto                                */
    char archs[8][16];          /* compatible_hardware list             */
    int  arch_count;
} store_pkg;

typedef struct store_model {
    store_pkg items[ST_MAX_PACKAGES];
    int       count;
    char      index_version[32];
    char      last_updated[64];
    int       from_cache;       /* 1 when served from local cache       */
} store_model;

void store_model_clear(store_model *m);

/* Package type helpers. */
pkg_type_t pkg_type_from_string(const char *s);   /* unknown -> PKG_GAME */
const char *pkg_type_to_string(pkg_type_t t);
/* i18n key for a type label ("store_type_game" ...). */
const char *pkg_type_locale_key(pkg_type_t t);

#endif /* DISCADIA_STORE_MODEL_H */
