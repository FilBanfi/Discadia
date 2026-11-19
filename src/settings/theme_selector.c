/*
 * theme_selector.c - lists every theme folder found under
 * <data_dir>/themes (plus the user themes dir), previewing display
 * name and description. Confirm applies the theme immediately:
 * app_reapply_theme() reloads colors/fonts/sounds, persists settings
 * and refreshes audio so the whole UI changes live.
 */

#include "app/app.h"
#include "ui/widgets.h"
#include "ui/renderer.h"
#include "ui/top_bar.h"
#include "i18n/locale.h"
#include "theme/theme.h"
#include "settings/settings_store.h"
#include "util/string_util.h"
#include "log.h"

#include <string.h>

#define MAX_THEMES 32

typedef struct { char id[64]; char name[128]; char desc[256]; } theme_entry;
static theme_entry s_themes[MAX_THEMES];
static int s_count;
static int s_selected;
static int s_top;            /* first visible row (scroll window) */

/* theme_list_available callback: collect entries into the static list. */
static int collect_cb(const char *dir_name, const char *display_name,
                      const char *description, void *ud)
{
    (void)ud;
    if (s_count >= MAX_THEMES) return 0;
    theme_entry *e = &s_themes[s_count++];
    str_safe_copy(e->id, dir_name, sizeof(e->id));
    str_safe_copy(e->name, (display_name && *display_name) ? display_name
                                                           : dir_name,
                  sizeof(e->name));
    str_safe_copy(e->desc, description ? description : "", sizeof(e->desc));
    return 0;
}

static void scan(app *a)
{
    s_count = 0;
    theme_list_available(app_data_dir(a), collect_cb, NULL);
    /* Keep selection on the currently active theme when possible. */
    const char *cur = app_settings(a)->theme;
    s_selected = 0;
    for (int i = 0; i < s_count; i++)
        if (strcmp(s_themes[i].id, cur) == 0) { s_selected = i; break; }
    s_top = 0;
}

static void ts_enter(app *a)  { scan(a); }
static void ts_leave(app *a)  { (void)a; }

static void ts_update(app *a, float dt_ms, const frame_input *in,
                      nav_request_t *out_nav)
{
    (void)dt_ms;
    if (in->back) { *out_nav = NAV_BACK; return; }

    if (in->up && s_selected > 0)
        { s_selected--; app_play_sound(a, "navigate"); }
    if (in->down && s_selected < s_count - 1)
        { s_selected++; app_play_sound(a, "navigate"); }

    if (in->confirm && s_count > 0)
    {
        app_play_sound(a, "select");
        str_safe_copy(app_settings(a)->theme, s_themes[s_selected].id,
                      sizeof(app_settings(a)->theme));
        app_reapply_theme(a);          /* reload + persist + audio refresh */
        scan(a);                       /* re-center selection on new theme */
    }
}

static void ts_draw(app *a, float dt_ms)
{
    discadia_theme *t = app_theme(a);
    layout *L = app_layout(a);
    widget_ctx w = app_widget_ctx(a);
    (void)dt_ms;

    renderer_draw_background(app_renderer(a), t);
    top_bar_draw(a, locale_get("theme"));

    int x = L->content_left;
    int wpx = L->content_right - L->content_left;
    int row_h = 56;
    int y0 = L->top_bar.h + 20;
    int visible = (L->sh - y0 - 90) / row_h;
    if (visible < 1) visible = 1;

    /* keep selection inside the scroll window */
    if (s_selected < s_top) s_top = s_selected;
    if (s_selected >= s_top + visible) s_top = s_selected - visible + 1;

    for (int i = s_top; i < s_count && i < s_top + visible; i++)
    {
        int is_cur = strcmp(s_themes[i].id, app_settings(a)->theme) == 0;
        widget_list_row(&w, x, y0 + (i - s_top) * row_h, wpx, row_h - 6,
                        s_themes[i].name, is_cur ? locale_get("installed")
                                                 : NULL,
                        i == s_selected);
    }

    /* Description of the focused theme at the bottom. */
    if (s_count > 0 && s_themes[s_selected].desc[0])
        widget_text_secondary(&w, FACE_BODY, t->sizes.description_size,
                              s_themes[s_selected].desc, L->sw / 2,
                              L->sh - 70, 0, wpx, 2);
}

const screen_vtable *screen_theme_selector_vtable(void)
{
    static const screen_vtable vt = {
        .name = "theme_selector", .enter = ts_enter, .update = ts_update,
        .draw = ts_draw, .leave = ts_leave
    };
    return &vt;
}
