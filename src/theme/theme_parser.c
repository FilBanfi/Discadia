/*
 * theme_parser.c - maps theme.json onto the discadia_theme struct.
 * Unknown keys are ignored (forward compatible); missing keys keep the
 * defaults already present in the struct.
 */

#include "theme_parser.h"
#include "util/json.h"
#include "util/fs.h"
#include "util/path_util.h"
#include "util/string_util.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Parse "#rgb", "#rrggbb", "#rrggbbaa" into SDL_Color. Returns 0 on success. */
static int parse_hex_color(const char *s, SDL_Color *out) {
    if (s == NULL || out == NULL || s[0] != '#') return -1;
    const char *hex = s + 1;
    size_t len = strlen(hex);
    unsigned int r = 0, g = 0, b = 0, a = 255;

    if (len == 3) {
        if (sscanf(hex, "%1x%1x%1x", &r, &g, &b) != 3) return -1;
        r *= 17; g *= 17; b *= 17;
    } else if (len == 6 || len == 8) {
        if (sscanf(hex, "%2x%2x%2x", &r, &g, &b) < 3) return -1;
        if (len == 8) sscanf(hex + 6, "%2x", &a);
    } else {
        return -1;
    }
    out->r = (Uint8)r; out->g = (Uint8)g; out->b = (Uint8)b; out->a = (Uint8)a;
    return 0;
}

static void copy_str(char *dst, size_t dst_size, const discadia_json *parent,
                     const char *key) {
    const char *v = json_get_string_or(parent, key, NULL);
    if (v != NULL) str_safe_copy(dst, v, dst_size);
}

static int parse_colors(discadia_theme *t, const discadia_json *colors) {
    static const struct { const char *key; int id; } map[] = {
        {"background", TCOL_BACKGROUND},
        {"background_gradient_end", TCOL_BACKGROUND_GRADIENT_END},
        {"primary", TCOL_PRIMARY},
        {"secondary", TCOL_SECONDARY},
        {"text", TCOL_TEXT},
        {"text_secondary", TCOL_TEXT_SECONDARY},
        {"accent", TCOL_ACCENT},
        {"selection_glow", TCOL_SELECTION_GLOW},
        {"toast_bg", TCOL_TOAST_BG},
        {"toast_border", TCOL_TOAST_BORDER},
    };
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        const char *hex = json_get_string_or(colors, map[i].key, NULL);
        if (hex != NULL) {
            parse_hex_color(hex, &t->colors[map[i].id]);
        }
    }
    return 0;
}


/* Read a numeric field from an object with default; frees the temp wrapper. */
static double obj_double(const discadia_json *obj, const char *key, double def) {
    discadia_json *child = json_get(obj, key);
    double v = json_double_or(child, def);
    json_free(child);
    return v;
}

int theme_parse_json(discadia_theme *theme, const char *theme_json_path) {
    if (theme == NULL || theme_json_path == NULL) return -1;

    discadia_json *root = json_parse_file(theme_json_path);
    if (root == NULL) {
        LOG_WARN("cannot parse %s", theme_json_path);
        return -1;
    }

    copy_str(theme->display_name, sizeof(theme->display_name), root, "name");
    copy_str(theme->author, sizeof(theme->author), root, "author");
    copy_str(theme->description, sizeof(theme->description), root, "description");

    discadia_json *colors = json_get(root, "colors");
    if (colors != NULL) { parse_colors(theme, colors); json_free(colors); }

    /* sizes */
    discadia_json *sizes = json_get(root, "sizes");
    if (sizes != NULL) {
        theme->sizes.icon_normal = json_get_int_or(sizes, "icon_normal", theme->sizes.icon_normal);
        theme->sizes.icon_selected = json_get_int_or(sizes, "icon_selected", theme->sizes.icon_selected);
        theme->sizes.icon_spacing = json_get_int_or(sizes, "icon_spacing", theme->sizes.icon_spacing);
        theme->sizes.icon_border_radius = json_get_int_or(sizes, "icon_border_radius", theme->sizes.icon_border_radius);
        theme->sizes.title_size = json_get_int_or(sizes, "title_size", theme->sizes.title_size);
        theme->sizes.description_size = json_get_int_or(sizes, "description_size", theme->sizes.description_size);
        theme->sizes.meta_size = json_get_int_or(sizes, "meta_size", theme->sizes.meta_size);
        theme->sizes.top_bar_height = json_get_int_or(sizes, "top_bar_height", theme->sizes.top_bar_height);
        theme->sizes.metadata_panel_height = json_get_int_or(sizes, "metadata_panel_height", theme->sizes.metadata_panel_height);
        theme->sizes.boot_title_size = json_get_int_or(sizes, "boot_title_size", theme->sizes.boot_title_size);
        json_free(sizes);
    }

    /* layout (relative 0..1 positions) */
    discadia_json *layout = json_get(root, "layout");
    if (layout != NULL) {
        theme->layout.icons_y = (float)obj_double(layout, "icons_y_position", theme->layout.icons_y);
        theme->layout.title_y = (float)obj_double(layout, "title_y_position", theme->layout.title_y);
        theme->layout.description_y = (float)obj_double(layout, "description_y_position", theme->layout.description_y);
        theme->layout.meta_y = (float)obj_double(layout, "meta_y_position", theme->layout.meta_y);
        theme->layout.top_bar_padding = json_get_int_or(layout, "top_bar_padding", theme->layout.top_bar_padding);
        json_free(layout);
    }

    /* animations */
    discadia_json *anim = json_get(root, "animations");
    if (anim != NULL) {
        theme->animations.enabled = json_get_bool_or(anim, "enabled", theme->animations.enabled);
        theme->animations.scroll_speed_ms = json_get_int_or(anim, "scroll_speed_ms", theme->animations.scroll_speed_ms);
        theme->animations.selection_scale_ms = json_get_int_or(anim, "selection_scale_ms", theme->animations.selection_scale_ms);
        theme->animations.fade_transition_ms = json_get_int_or(anim, "fade_transition_ms", theme->animations.fade_transition_ms);
        json_free(anim);
    }

    /* visual effects */
    discadia_json *fx = json_get(root, "visual_effects");
    if (fx != NULL) {
        theme->effects.icon_shadow = json_get_bool_or(fx, "icon_shadow", theme->effects.icon_shadow);
        theme->effects.selection_glow = json_get_bool_or(fx, "selection_glow", theme->effects.selection_glow);
        theme->effects.scanline_overlay = json_get_bool_or(fx, "scanline_overlay", theme->effects.scanline_overlay);
        copy_str(theme->effects.frame_overlay, sizeof(theme->effects.frame_overlay), fx, "frame_overlay");
        json_free(fx);
    }

    /* sounds: fixed logical slots */
    discadia_json *snd = json_get(root, "sounds");
    if (snd != NULL) {
        theme->sounds.enabled = json_get_bool_or(snd, "enabled", theme->sounds.enabled);
        theme->sounds.volume = (float)obj_double(snd, "volume", theme->sounds.volume);
        static const char *slots[THEME_MAX_SOUNDS] =
            { "navigate", "select", "back", "cd_insert", "boot" };
        theme->sounds.sound_count = THEME_MAX_SOUNDS;
        for (int i = 0; i < THEME_MAX_SOUNDS; i++) {
            str_safe_copy(theme->sounds.sound_names[i], slots[i],
                          sizeof(theme->sounds.sound_names[i]));
            copy_str(theme->sounds.sound_paths[i],
                     sizeof(theme->sounds.sound_paths[i]), snd, slots[i]);
        }
        json_free(snd);
    }

    /* metadata display toggles */
    discadia_json *md = json_get(root, "metadata_display");
    if (md != NULL) {
        theme->metadata_display.show_platform = json_get_bool_or(md, "show_platform", 1);
        theme->metadata_display.show_year = json_get_bool_or(md, "show_year", 1);
        theme->metadata_display.show_publisher = json_get_bool_or(md, "show_publisher", 1);
        theme->metadata_display.show_developer = json_get_bool_or(md, "show_developer", 1);
        theme->metadata_display.show_genre = json_get_bool_or(md, "show_genre", 1);
        theme->metadata_display.show_players = json_get_bool_or(md, "show_players", 1);
        theme->metadata_display.show_version = json_get_bool_or(md, "show_version", 1);
        theme->metadata_display.show_region = json_get_bool_or(md, "show_region", 1);
        copy_str(theme->metadata_display.separator,
                 sizeof(theme->metadata_display.separator), md, "separator");
        json_free(md);
    }

    /* notifications */
    discadia_json *notif = json_get(root, "notifications");
    if (notif != NULL) {
        theme->toast_timeout_ms = json_get_int_or(notif, "toast_timeout_ms", theme->toast_timeout_ms);
        json_free(notif);
    }

    /* fonts (paths resolved later) */
    discadia_json *fonts = json_get(root, "fonts");
    if (fonts != NULL) {
        copy_str(theme->font_paths.title_font, sizeof(theme->font_paths.title_font), fonts, "title");
        copy_str(theme->font_paths.body_font, sizeof(theme->font_paths.body_font), fonts, "body");
        copy_str(theme->font_paths.small_font, sizeof(theme->font_paths.small_font), fonts, "small");
        json_free(fonts);
    }

    json_free(root);
    return 0;
}

/* Resolve one relative path against the theme root. If it is absolute or
 * does not exist on disk, clear the buffer so callers use fallbacks. */
static void resolve_asset(char *path_io, size_t size, const char *root) {
    if (str_is_empty(path_io)) return;

    if (!path_is_absolute(path_io)) {
        char full[1536];
        path_join(full, sizeof(full), root, path_io);
        str_safe_copy(path_io, full, size);
    }
    if (!fs_file_exists(path_io)) {
        LOG_DEBUG("theme asset missing: %s", path_io);
        path_io[0] = '\0';
    }
}

void theme_resolve_assets(discadia_theme *theme) {
    if (theme == NULL || str_is_empty(theme->root_path)) return;

    resolve_asset(theme->font_paths.title_font,
                  sizeof(theme->font_paths.title_font), theme->root_path);
    resolve_asset(theme->font_paths.body_font,
                  sizeof(theme->font_paths.body_font), theme->root_path);
    resolve_asset(theme->font_paths.small_font,
                  sizeof(theme->font_paths.small_font), theme->root_path);
    resolve_asset(theme->effects.frame_overlay,
                  sizeof(theme->effects.frame_overlay), theme->root_path);

    for (int i = 0; i < theme->sounds.sound_count; i++) {
        resolve_asset(theme->sounds.sound_paths[i],
                      sizeof(theme->sounds.sound_paths[i]), theme->root_path);
    }
}
