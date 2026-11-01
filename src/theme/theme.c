/*
 * theme.c - theme loading orchestration.
 * Depends only on util + cJSON; parsing itself lives in theme_parser.c.
 */

#include "theme.h"
#include "theme_parser.h"
#include "util/fs.h"
#include "util/path_util.h"
#include "util/string_util.h"
#include "log.h"

#include <string.h>

void theme_set_defaults(discadia_theme *theme) {
    if (theme == NULL) return;
    memset(theme, 0, sizeof(*theme));

    str_safe_copy(theme->name, "default", sizeof(theme->name));
    str_safe_copy(theme->display_name, "Discadia Default", sizeof(theme->display_name));

    /* Arcade-classic-like defaults so the UI is usable with zero files. */
    theme->colors[TCOL_BACKGROUND]              = (SDL_Color){0x1a, 0x1a, 0x2e, 0xff};
    theme->colors[TCOL_BACKGROUND_GRADIENT_END] = (SDL_Color){0x16, 0x21, 0x3e, 0xff};
    theme->colors[TCOL_PRIMARY]                 = (SDL_Color){0xe9, 0x45, 0x60, 0xff};
    theme->colors[TCOL_SECONDARY]               = (SDL_Color){0x0f, 0x34, 0x60, 0xff};
    theme->colors[TCOL_TEXT]                    = (SDL_Color){0xff, 0xff, 0xff, 0xff};
    theme->colors[TCOL_TEXT_SECONDARY]          = (SDL_Color){0xa0, 0xa0, 0xa0, 0xff};
    theme->colors[TCOL_ACCENT]                  = (SDL_Color){0xf5, 0xa6, 0x23, 0xff};
    theme->colors[TCOL_SELECTION_GLOW]          = (SDL_Color){0xf5, 0xa6, 0x23, 0xff};
    theme->colors[TCOL_TOAST_BG]                = (SDL_Color){0x10, 0x10, 0x18, 0xf0};
    theme->colors[TCOL_TOAST_BORDER]            = (SDL_Color){0xf5, 0xa6, 0x23, 0xff};

    theme->sizes.icon_normal = 180;
    theme->sizes.icon_selected = 240;
    theme->sizes.icon_spacing = 220;
    theme->sizes.icon_border_radius = 8;
    theme->sizes.title_size = 56;
    theme->sizes.description_size = 22;
    theme->sizes.meta_size = 18;
    theme->sizes.top_bar_height = 80;
    theme->sizes.metadata_panel_height = 200;
    theme->sizes.boot_title_size = 96;

    theme->layout.icons_y = 0.55f;
    theme->layout.title_y = 0.12f;
    theme->layout.description_y = 0.78f;
    theme->layout.meta_y = 0.88f;
    theme->layout.top_bar_padding = 40;

    theme->animations.enabled = 1;
    theme->animations.scroll_speed_ms = 150;
    theme->animations.selection_scale_ms = 200;
    theme->animations.fade_transition_ms = 300;

    theme->effects.icon_shadow = 1;
    theme->effects.selection_glow = 1;
    theme->effects.scanline_overlay = 0;

    theme->sounds.enabled = 1;   /* ON by default per user decision */
    theme->sounds.volume = 0.7f;

    theme->metadata_display.show_platform = 1;
    theme->metadata_display.show_year = 1;
    theme->metadata_display.show_publisher = 1;
    theme->metadata_display.show_developer = 1;
    theme->metadata_display.show_genre = 1;
    theme->metadata_display.show_players = 1;
    theme->metadata_display.show_version = 1;
    theme->metadata_display.show_region = 1;
    str_safe_copy(theme->metadata_display.separator, " \xE2\x80\xA2 ",
                  sizeof(theme->metadata_display.separator));

    theme->toast_timeout_ms = 3000;
}

int theme_load(discadia_theme *theme, const char *data_dir, const char *theme_id) {
    if (theme == NULL || data_dir == NULL) return -1;

    theme_set_defaults(theme);
    if (!str_is_empty(theme_id)) {
        str_safe_copy(theme->name, theme_id, sizeof(theme->name));
    }

    char themes_dir[512];
    path_join(themes_dir, sizeof(themes_dir), data_dir, "themes");

    char root[1024];
    path_join(root, sizeof(root), themes_dir, theme->name);

    if (!fs_dir_exists(root)) {
        LOG_WARN("theme '%s' not found in %s, using built-in defaults",
                 theme->name, themes_dir);
        return 0; /* graceful: defaults are a valid theme */
    }
    str_safe_copy(theme->root_path, root, sizeof(theme->root_path));

    char theme_json[1536];
    path_join(theme_json, sizeof(theme_json), root, "theme.json");

    if (!fs_file_exists(theme_json)) {
        LOG_WARN("theme '%s': missing theme.json, using defaults", theme->name);
        return 0;
    }

    if (theme_parse_json(theme, theme_json) != 0) {
        LOG_WARN("theme '%s': parse error, using defaults", theme->name);
        theme_set_defaults(theme);
        str_safe_copy(theme->name, theme_id ? theme_id : "default", sizeof(theme->name));
        str_safe_copy(theme->root_path, root, sizeof(theme->root_path));
        return 0;
    }

    theme_resolve_assets(theme);
    LOG_DEBUG("theme loaded: %s (%d sounds)", theme->display_name,
              theme->sounds.sound_count);
    return 0;
}

struct list_ctx {
    const char *data_dir;
    theme_list_cb cb;
    void *userdata;
    int count;
};

static int theme_dir_cb(const char *name, const char *full_path, int is_dir,
                        void *userdata) {
    struct list_ctx *ctx = (struct list_ctx *)userdata;
    if (!is_dir || ctx == NULL || ctx->cb == NULL) return 0;

    char tj[1536];
    path_join(tj, sizeof(tj), full_path, "theme.json");

    discadia_theme tmp;
    theme_set_defaults(&tmp);
    if (fs_file_exists(tj)) {
        theme_parse_json(&tmp, tj);
    }

    if (ctx->cb(name, tmp.display_name, tmp.description, ctx->userdata) != 0) {
        return 1; /* stop iteration */
    }
    ctx->count++;
    return 0;
}

int theme_list_available(const char *data_dir, theme_list_cb cb, void *userdata) {
    if (data_dir == NULL || cb == NULL) return -1;
    char themes_dir[512];
    path_join(themes_dir, sizeof(themes_dir), data_dir, "themes");

    struct list_ctx ctx = { data_dir, cb, userdata, 0 };
    if (fs_list_dir(themes_dir, theme_dir_cb, &ctx) != 0) return -1;
    return ctx.count;
}

SDL_Color theme_color(const discadia_theme *t, int id) {
    if (t == NULL || id < 0 || id >= TCOL_COUNT) {
        return (SDL_Color){0, 0, 0, 255};
    }
    return t->colors[id];
}

const char *theme_sound_path(const discadia_theme *t, const char *logical_name) {
    if (t == NULL || logical_name == NULL) return "";
    for (int i = 0; i < t->sounds.sound_count; i++) {
        if (strcmp(t->sounds.sound_names[i], logical_name) == 0) {
            return t->sounds.sound_paths[i];
        }
    }
    return "";
}
