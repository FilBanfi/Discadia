/*
 * store_model.c - plain data helpers for the catalog structs. Kept free
 * of SDL/IO so the parsing layer and UI share one definition of "type".
 */

#include "store/store_model.h"
#include "util/string_util.h"

#include <string.h>

void store_model_clear(store_model *m)
{
    if (!m) return;
    memset(m, 0, sizeof(*m));
}

pkg_type_t pkg_type_from_string(const char *s)
{
    if (!s) return PKG_GAME;
    if (strcmp(s, "theme")  == 0) return PKG_THEME;
    if (strcmp(s, "engine") == 0) return PKG_ENGINE;
    if (strcmp(s, "app")    == 0) return PKG_APP;
    return PKG_GAME;                      /* unknown -> conservative default */
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
