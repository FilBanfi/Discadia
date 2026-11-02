/*
 * theme_selector.c - list themes found under <data_dir>/themes plus the
 * built-in "arcade-classic" default. Selecting one applies it live,
 * saves the choice and shows a toast on fallback (never crashes).
 */

#include <string.h>
#include "app/app.h"
#include "audio/audio.h"
#include "settings/settings_store.h"
#include "theme/theme_parser.h"
#include "ui/widgets.h"
#include "ui/renderer.h"
#include "ui/toast.h"
#include "i18n/locale.h"
#include "util/fs.h"
#include "util/path_util.h"
#include "util/string_util.h"
#include "log.h"

#define MAX_THEMES 32

typedef struct { char id[64]; char name[128]; char desc[256]; } theme_entry;
static theme_entry s_list[MAX_THEMES];
static int s_count = 0;
static int s_sel = 0;
static int s_scroll = 0;

static int collect_cb(const char *dir_name, const char *display_name,
                      const char *description, void *userdata)
{
    (void)userdata;
    if (s_count >= MAX_THEMES) return 1;
    str_safe_copy(s_list[s_count].id, dir_name, sizeof(s_list[0].id));
    str_safe_copy(s_list[s_count].name,
                  display_name && *display_name ? display_name : dir_name,
                  sizeof(s_list[0].name));
    str_safe_copy(s_list[s_count].desc, description ? description : "",
                  sizeof(s_list[0].desc));
    s_count++;
    return 0;
}

static void rebuild_list(const char *data_dir)
{
    s_count = 0;
    /* Built-in default always selectable even if folder is missing. */
    str_safe_copy(s_list[0].id, "arcade-classic", sizeof(s_list[0].id));
    str_safe_copy(s_list[0].name, "Arcade Classic", sizeof(s_list[0].name));
    str_safe_copy(s_list[0].desc, "", sizeof(s_list[0].desc));
    s_count = 1;
    char tdir[512];
    path_join(tdir, sizeof(tdir), data_dir, "themes");
    theme_list_available(tdir[0] ? tdir : data_dir, collect_cb, NULL);
    /* De-duplicate by id keeping the parsed entry (better name). */
    for (int i = s_count - 1; i >= 1; i--)
        for (int j = 0; j < i; j++)
            if (!strcmp(s_list[i].id, s_list[j].id))
            {
                memmove(&s_list[i], &s_list[i + 1],
                        (size_t)(s_count - i - 1) * sizeof(theme_entry));
                s_count--;
                break;
            }
}

static void apply_theme(app *a, const char *id)
{
    settings_model *s = app_settings(a);
    str_safe_copy(s->theme, id, sizeof(s->theme));
    discadia_theme probe;
    theme_set_defaults(&probe);
    if (theme_load(&probe, app_data_dir(a), id) != 0 ||
        strcmp(probe.name, id) != 0)
    {
        char msg[256];
        locale_fmt(msg, sizeof(msg), "error_theme_unavailable",
                   "theme", id, NULL, NULL);
        toast_push_error(app_toasts(a), msg);
        str_safe_copy(s->theme, "arcade-classic", sizeof(s->theme));
    }
    app_reapply_theme(a);
    settings_store_save(s);
    app_play_sound(a, "select");
}

static void ts_enter(app *a)
{
    rebuild_list(app_data_dir(a));
    s_sel = 0;
    for (int i = 0; i < s_count; i++)
        if (!strcmp(s_list[i].id, app_settings(a)->theme)) s_sel = i;
    s_scroll = 0;
}

static void ts_update(app *a, float dt_ms, const frame_input *in,
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
        apply_theme(a, s_list[s_sel].id);
        char cur[64];
        str_safe_copy(cur, app_settings(a)->theme, sizeof(cur));
        rebuild_list(app_data_dir(a));   /* active marker refreshes */
    }
}

static void ts_draw(app *a, float dt_ms)
{
    (void)dt_ms;
    discadia_theme *t = app_theme(a);
    layout *L = app_layout(a);
    widget_ctx w = app_widget_ctx(a);

    renderer_draw_background(app_renderer(a), t);
    top_bar_draw(a, locale_get("choose_theme"));

    int row_h = t->sizes.title_size + 26;
    int y0 = L->top_bar.y + L->top_bar.h + 20;
    int x = L->content_left, wp = L->content_right - L->content_left;

    for (int i = s_scroll; i < s_count; i++)
    {
        if (y0 + (i - s_scroll) * row_h > L->sh - 60) break;
        char val[160];
        val[0] = '\0';
        if (!strcmp(s_list[i].id, t->name))
            str_safe_copy(val, locale_get("installed"), sizeof(val));
        else if (s_list[i].desc[0])
            str_safe_copy(val, s_list[i].desc, sizeof(val));
        widget_list_row(&w, x, y0 + (i - s_scroll) * row_h, wp, row_h - 8,
                        s_list[i].name, val, i == s_sel);
    }
}

static void ts_leave(app *a) { (void)a; }

const screen_vtable *screen_theme_selector_vtable(void)
{
    static const screen_vtable vt = {
        .name = "theme_selector", .enter = ts_enter, .update = ts_update,
        .draw = ts_draw, .leave = ts_leave
    };
    return &vt;
}
