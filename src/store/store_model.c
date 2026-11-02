/*
 * store_model.c - plain data helpers for the store catalog.
 * No SDL, no I/O: fully unit-testable headless.
 */

#include <string.h>
#include "store/store_model.h"

void store_model_clear(store_model *m)
{
    if (!m) return;
    memset(m, 0, sizeof(*m));
}

pkg_type_t pkg_type_from_string(const char *s)
{
    if (!s) return PKG_GAME;
    if (!strcmp(s, "theme"))  return PKG_THEME;
    if (!strcmp(s, "engine")) return PKG_ENGINE;
    if (!strcmp(s, "app"))    return PKG_APP;
    return PKG_GAME;   /* unknown types degrade to game, never crash */
}

const char *pkg_type_to_string(pkg_type_t t)
{
    switch (t)
    {
        case PKG_THEME:  return "theme";
        case PKG_ENGINE: return "engine";
        case PKG_APP:    return "app";
        default:         return "game";
    }
}

const char *pkg_type_locale_key(pkg_type_t t)
{
    switch (t)
    {
        case PKG_THEME:  return "store_type_theme";
        case PKG_ENGINE: return "store_type_engine";
        case PKG_APP:    return "store_type_app";
        default:         return "store_type_game";
    }
}
