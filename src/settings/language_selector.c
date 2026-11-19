/*
 * language_selector.c - lists locales available under
 * <data_dir>/locales, applies the choice with locale_set() (which also
 * reloads the translation table) and persists it. The UI is redrawn in
 * the new language immediately: no restart required.
 */

#include "app/app.h"
#include "ui/widgets.h"
#include "ui/renderer.h"
#include "ui/top_bar.h"
#include "i18n/locale.h"
#include "settings/settings_store.h"
#include "util/string_util.h"
#include "log.h"

#include <string.h>

#define MAX_LANGS 16

typedef struct { char code[16]; char native[96]; } lang_entry;
static lang_entry s_langs[MAX_LANGS];
static int s_count, s_selected;

static int collect_cb(const char *code, const char *native_name, void *ud)
{
    (void)ud;
    if (s_count >= MAX_LANGS) return 0;
    str_safe_copy(s_langs[s_count].code, code, sizeof(s_langs[0].code));
    str_safe_copy(s_langs[s_count].native,
                  (native_name && *native_name) ? native_name : code,
                  sizeof(s_langs[0].native));
    s_count++;
    return 0;
}

static void scan(app *a)
{
    s_count = 0;
    locale_list_available(app_data_dir(a), collect_cb, NULL);
    const char *cur = locale_get_current();
    s_selected = 0;
    for (int i = 0; i < s_count; i++)
        if (strcmp(s_langs[i].code, cur) == 0) { s_selected = i; break; }
}

static void ls_enter(app *a) { scan(a); }
static void ls_leave(app *a) { (void)a; }

static void ls_update(app *a, float dt_ms, const frame_input *in,
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
        if (locale_set(app_data_dir(a), s_langs[s_selected].code) != 0)
        {
            app_play_sound(a, "back");   /* unavailable -> keep old */
            return;
        }
        str_safe_copy(app_settings(a)->language, s_langs[s_selected].code,
                      sizeof(app_settings(a)->language));
        settings_store_save(app_settings(a));
        app_play_sound(a, "select");     /* plays with the NEW strings loaded */
    }
}

static void ls_draw(app *a, float dt_ms)
{
    discadia_theme *t = app_theme(a);
    layout *L = app_layout(a);
    widget_ctx w = app_widget_ctx(a);
    (void)dt_ms;

    renderer_draw_background(app_renderer(a), t);
    top_bar_draw(a, locale_get("language"));

    int x = L->content_left;
    int wpx = L->content_right - L->content_left;
    int row_h = 56;
    int y0 = L->top_bar.h + 24;

    for (int i = 0; i < s_count; i++)
    {
        int is_cur = strcmp(s_langs[i].code, locale_get_current()) == 0;
        widget_list_row(&w, x, y0 + i * row_h, wpx, row_h - 6,
                        s_langs[i].native,
                        is_cur ? locale_get("default") : NULL,
                        i == s_selected);
    }
}

const screen_vtable *screen_language_selector_vtable(void)
{
    static const screen_vtable vt = {
        .name = "language_selector", .enter = ls_enter, .update = ls_update,
        .draw = ls_draw, .leave = ls_leave
    };
    return &vt;
}
