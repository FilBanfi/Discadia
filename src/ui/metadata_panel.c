/*
 * metadata_panel.c - bottom panel under the carousel. Field order and
 * visibility come from theme.metadata_display; each known key maps to a
 * localized label + item value. Unknown keys are ignored (forward
 * compatible with new themes).
 */

#include "ui/metadata_panel.h"
#include "ui/widgets.h"
#include "i18n/locale.h"
#include "util/string_util.h"

#include <stdio.h>
#include <string.h>

static const char *meta_label(const char *key)
{
    if (!strcmp(key, "platform"))    return locale_get("store_type_game");
    if (!strcmp(key, "year"))        return locale_get("version_label");
    if (!strcmp(key, "genre"))       return locale_get("category");
    if (!strcmp(key, "players"))     return locale_get("players");
    if (!strcmp(key, "developer"))   return locale_get("description_label");
    if (!strcmp(key, "engine"))      return locale_get("engine");
    return NULL;
}

static void meta_value(const library_item *it, const char *key,
                       char *buf, size_t sz)
{
    buf[0] = '\0';
    if (!strcmp(key, "platform"))  str_safe_copy(buf, it->platform, sz);
    else if (!strcmp(key, "year"))
    {
        if (it->release_year > 0) snprintf(buf, sz, "%d", it->release_year);
    }
    else if (!strcmp(key, "genre"))     str_safe_copy(buf, it->genre, sz);
    else if (!strcmp(key, "players"))
    {
        if (it->max_players == 1) str_safe_copy(buf, locale_get("player"), sz);
        else if (it->max_players > 1)
            locale_fmt(buf, sz, "players", "value", NULL, NULL, NULL),
            snprintf(buf, sz, "%d %s", it->max_players,
                     locale_get("players"));
    }
    else if (!strcmp(key, "developer")) str_safe_copy(buf, it->developer, sz);
    else if (!strcmp(key, "engine"))    str_safe_copy(buf, it->engine, sz);
}

void metadata_panel_draw(app *a, const library_item *item)
{
    if (!item) return;
    layout *L = app_layout(a);
    discadia_theme *t = app_theme(a);
    widget_ctx w = app_widget_ctx(a);

    /* Description block, wrapped to panel width. */
    int y = L->desc_y;
    if (item->description[0])
        y += widget_text_secondary(&w, FACE_BODY, t->sizes.description_size,
                                   item->description, L->content_x, y, 2,
                                   L->sw - 2 * L->content_x,
                                   t->metadata_desc_max_lines);
    y += 6;

    /* Meta fields row per theme preference order. */
    int x = L->content_x;
    for (int i = 0; i < t->metadata_display_count && i < 8; i++)
    {
        const char *key = t->metadata_display[i];
        char val[128];
        meta_value(item, key, val, sizeof(val));
        if (!val[0]) continue;
        const char *label = meta_label(key);
        char line[192];
        snprintf(line, sizeof(line), "%s: %s", label ? label : key, val);
        int fs = t->sizes.meta_size;
        int h = widget_text(&w, FACE_SMALL, fs,
                            theme_color(t, TCOL_TEXT_SECONDARY),
                            line, x, y, 2, 0, 1);
        int tw = (int)(strlen(line) * fs * 0.58);
        x += tw + 24;
        if (x > L->sw - 100) break;   /* overflow: drop the rest */
        (void)h;
    }
}
