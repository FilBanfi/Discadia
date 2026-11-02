/*
 * language_selector.c - list locales found under <data_dir>/locales.
 * Selecting one reloads the i18n tables live, saves the choice and
 * shows a toast when the requested language had to fall back to "en".
 */

#include <string.h>
#include "ui/top_bar.h"
#include "app/app.h"
#include "audio/audio.h"
#include "settings/settings_store.h"
#include "ui/widgets.h"
#include "ui/renderer.h"
#include "ui/toast.h"
#include "i18n/locale.h"
#include "util/string_util.h"
#include "log.h"

#define MAX_LANGS 32

typedef struct { char code[16]; char native[64]; } lang_entry;
static lang_entry s_list[MAX_LANGS];
static int s_count = 0;
static int s_sel = 0;
static int s_scroll = 0;

static int collect_cb(const char *code, const char *native_name, void *ud)
{
    (void)ud;
    if (s_count >= MAX_LANGS) return 1;
    str_safe_copy(s_list[s_count].code, code, sizeof(s_list[0].code));
    str_safe_copy(s_list[s_count].native,
                  native_name && *native_name ? native_name : code,
                  sizeof(s_list[0].native));
    s_count++;
    return 0;
}

static void rebuild_list(const char *data_dir)
{
    s_count = 0;
    locale_list_available(data_dir, collect_cb, NULL);
    /* Guarantee at least English even when the scan fails entirely. */
    if (s_count == 0)
    {
        str_safe_copy(s_list[0].code, "en", sizeof(s_list[0].code));
        str_safe_copy(s_list[0].native, "English", sizeof(s_list[0].native));
        s_count = 1;
    }
}

static void apply_language(app *a, const char *code)
{
    settings_model *s = app_settings(a);
    str_safe_copy(s->language, code, sizeof(s->language));
    if (locale_set(app_data_dir(a), code) != 0)
    {
        char msg[256];
        locale_fmt(msg, sizeof(msg), "error_language_unavailable",
                   "language", code, NULL, NULL);
        toast_push_error(app_toasts(a), msg);
        str_safe_copy(s->language, "en", sizeof(s->language));
        locale_set(app_data_dir(a), "en");
    }
    settings_store_save(s);
    app_play_sound(a, "select");
}

static void ls_enter(app *a)
{
    rebuild_list(app_data_dir(a));
    s_sel = 0;
    for (int i = 0; i < s_count; i++)
        if (!strcmp(s_list[i].code, app_settings(a)->language)) s_sel = i;
    s_scroll = 0;
}

static void ls_update(app *a, float dt_ms, const frame_input *in,
                      nav_request_t *out_nav)
{
    (void)dt_ms;
    if (in->back) { *out_nav = NAV_BACK; return; }
    if (in->up && s_sel > 0) { s_sel--; app_play_sound(a, "navigate"); }
    if (in->down && s_sel < s_count - 1) { s_sel++; app_play_sound(a, "navigate"); }

    int visible = (app_layout(a)->sh - 200) /
                  (app_theme(a)->sizes.title_size + 26);
    if (visible < 3) visible = 3;
    if (s_sel < s_scroll) s_scroll = s_sel;
    if (s_sel >= s_scroll + visible) s_scroll = s_sel - visible + 1;

    if (in->confirm && s_count > 0)
    {
        apply_language(a, s_list[s_sel].code);
        rebuild_list(app_data_dir(a));   /* labels re-resolve in new lang */
    }
}

static void ls_draw(app *a, float dt_ms)
{
    (void)dt_ms;
    discadia_theme *t = app_theme(a);
    layout *L = app_layout(a);
    widget_ctx w = app_widget_ctx(a);

    renderer_draw_background(app_renderer(a), t);
    top_bar_draw(a, locale_get("settings_language"));

    int row_h = t->sizes.title_size + 26;
    int y0 = L->top_bar.y + L->top_bar.h + 20;
    int x = L->content_left, wp = L->content_right - L->content_left;

    for (int i = s_scroll; i < s_count; i++)
    {
        if (y0 + (i - s_scroll) * row_h > L->sh - 60) break;
        char val[64];
        val[0] = '\0';
        if (!strcmp(s_list[i].code, locale_get_current()))
            str_safe_copy(val, locale_get("installed"), sizeof(val));
        widget_list_row(&w, x, y0 + (i - s_scroll) * row_h, wp, row_h - 8,
                        s_list[i].native, val, i == s_sel);
    }
}

static void ls_leave(app *a) { (void)a; }

const screen_vtable *screen_language_selector_vtable(void)
{
    static const screen_vtable vt = {
        .name = "language_selector", .enter = ls_enter, .update = ls_update,
        .draw = ls_draw, .leave = ls_leave
    };
    return &vt;
}
