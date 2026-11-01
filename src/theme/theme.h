#ifndef DISCADIA_THEME_H
#define DISCADIA_THEME_H

/*
 * Theme system: all colors, sizes, layout positions, animation timings,
 * sounds and visual effects come from theme.json. Nothing in the UI code
 * may hardcode any of these values; everything reads from this struct.
 *
 * A theme lives in <data_dir>/themes/<name>/theme.json with optional
 * fonts/, sounds/ and assets/ subfolders. Missing assets fall back to
 * procedural drawing so a theme without any file still works.
 */

#include <SDL.h>

#define TCOL_BACKGROUND              0
#define TCOL_BACKGROUND_GRADIENT_END 1
#define TCOL_PRIMARY                 2
#define TCOL_SECONDARY               3
#define TCOL_TEXT                    4
#define TCOL_TEXT_SECONDARY          5
#define TCOL_ACCENT                  6
#define TCOL_SELECTION_GLOW          7
#define TCOL_TOAST_BG                8
#define TCOL_TOAST_BORDER            9
#define TCOL_COUNT                   10

#define THEME_MAX_SOUNDS 5

typedef struct {
    char name[64];            /* internal id, e.g. "arcade-classic" */
    char display_name[128];   /* human readable name                */
    char description[256];
    char author[128];
    char root_path[512];      /* absolute path of theme folder      */

    SDL_Color colors[TCOL_COUNT];

    struct {
        int icon_normal;
        int icon_selected;
        int icon_spacing;
        int icon_border_radius;
        int title_size;
        int description_size;
        int meta_size;
        int top_bar_height;
        int metadata_panel_height;
        int boot_title_size;
    } sizes;

    struct {
        float icons_y;
        float title_y;
        float description_y;
        float meta_y;
        int   top_bar_padding;
    } layout;

    struct {
        int enabled;
        int scroll_speed_ms;
        int selection_scale_ms;
        int fade_transition_ms;
    } animations;

    struct {
        int  icon_shadow;
        int  selection_glow;
        int  scanline_overlay;
        char frame_overlay[256];   /* relative path or empty */
    } effects;

    struct {
        int   enabled;
        float volume;              /* 0..1 */
        char  sound_paths[THEME_MAX_SOUNDS][512];
        char  sound_names[THEME_MAX_SOUNDS][32];
        int   sound_count;
    } sounds;

    struct {
        int show_platform, show_year, show_publisher, show_developer;
        int show_genre, show_players, show_version, show_region;
        char separator[32];
    } metadata_display;

    struct {
        char title_font[512];   /* resolved absolute or empty -> fallback */
        char body_font[512];
        char small_font[512];
    } font_paths;

    int toast_timeout_ms;       /* notification lifetime */
} discadia_theme;

/* Sound ids used by the UI (index into theme.sounds). */
enum { SND_NAVIGATE = 0, SND_SELECT, SND_BACK, SND_CD_INSERT, SND_BOOT };

/* Fill the theme with built-in defaults (used before parsing / on error). */
void theme_set_defaults(discadia_theme *theme);

/* Load theme by id from data_dir (e.g. /etc/discadia or ./data).
 * Falls back to built-in defaults when files are missing. Returns 0 on
 * success, -1 only for allocation errors. */
int theme_load(discadia_theme *theme, const char *data_dir, const char *theme_id);

/* List available themes under data_dir/themes. Callback receives the
 * theme folder name plus parsed display name/description.
 * Returns number found (or -1 on error). */
typedef int (*theme_list_cb)(const char *dir_name, const char *display_name,
                             const char *description, void *userdata);
int theme_list_available(const char *data_dir, theme_list_cb cb, void *userdata);

/* Get a color; returns black if id out of range. */
SDL_Color theme_color(const discadia_theme *t, int id);

/* Find a sound path by logical name ("navigate", ...). Returns "" if none. */
const char *theme_sound_path(const discadia_theme *t, const char *logical_name);

#endif /* DISCADIA_THEME_H */
