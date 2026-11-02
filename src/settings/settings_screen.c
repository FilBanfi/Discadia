/*
 * settings_screen.c - main settings list.
 *
 * Rows: Theme / Language / Button Remap / Audio / Volume / Animations /
 * Clock format / Confirm shutdown / Store auto-update / System info.
 * Values are edited in place with LEFT/RIGHT where sensible; entries
 * that open sub-screens use CONFIRM. Every change is persisted
 * immediately (atomic save) so a power cut never loses settings.
 */

#include "app/app.h"
#include "settings/settings_store.h"
#include "ui/widgets.h"
#include "ui/renderer.h"
#include "i18n/locale.h"
#include "system/time_service.h"
#include "util/string_util.h"
#include "log.h"

typedef enum set_row {
    SR_THEME = 0,
    SR_LANGUAGE,
    SR_REMAP,
    SR_AUDIO,
    SR_VOLUME,
    SR_ANIMATIONS,
    SR_CLOCK_FORMAT,
    SR_CONFIRM_SHUTDOWN,
    SR_STORE_AUTO,
    SR_COUNT
} set_row_t;

static const char *SR_LABEL_KEY[SR_COUNT] = {
    "settings_theme", "settings_language", "settings_input_remap",
    "settings_audio", "settings_volume", "settings_animations",
    "clock_format", "settings_confirm_shutdown",
    "settings_store_auto_update"
};

static int s_sel = 0;
static int s_scroll = 0;   /* first visible row */

static void settings_enter(app *a) { (void)a; }
static void settings_leave(app *a) { (void)a; }

static const char *onoff(int v)
{
    return locale_get(v ? "on" : "off");
}

static void value_for_row(app *a, int row, char *buf, size_t sz)
{
    settings_model *s = app_settings(a);
    buf[0] = '\0';
    switch (row)
    {
        case SR_THEME:       str_safe_copy(buf, s->theme, sz); break;
        case SR_LANGUAGE:    str_safe_copy(buf, s->language, sz); break;
        case SR_REMAP:       break;                          /* opens UI  */
        case SR_AUDIO:       str_safe_copy(buf, onoff(s->audio_enabled), sz); break;
        case SR_VOLUME:      str_format(buf, sz, "%d%%",
                                        (int)(s->audio_volume * 100.f + 0.5f)); break;
        case SR_ANIMATIONS:  str_safe_copy(buf, onoff(s->animations_enabled), sz); break;
        case SR_CLOCK_FORMAT:str_safe_copy(buf,
                             s->clock_format == CLOCK_FORMAT_12H
                                 ? locale_get("format_12h")
                                 : locale_get("format_24h"), sz); break;
        case SR_CONFIRM_SHUTDOWN:
            str_safe_copy(buf, onoff(s->confirm_shutdown), sz); break;
        case SR_STORE_AUTO:
            str_safe_copy(buf, onoff(s->store_auto_update_index), sz); break;
        default: break;
    }
}

/* Toggle / adjust the value of an inline-editable row. delta = +/-1. */
static int row_adjust(app *a, int row, int delta)
{
    settings_model *s = app_settings(a);
    switch (row)
    {
        case SR_AUDIO:          s->audio_enabled = !s->audio_enabled;
                                audio_set_enabled(app_audio_sys(a),
                                                  s->audio_enabled &&
                                                  app_theme(a)->sounds.enabled);
                                break;
        case SR_VOLUME:
        {
            float v = s->audio_volume + delta * 0.1f;
            if (v < 0.f) v = 0.f;
            if (v > 1.f) v = 1.f;
            s->audio_volume = v;
            audio_set_volume(app_audio_sys(a), v);
            break;
        }
        case SR_ANIMATIONS:     s->animations_enabled = !s->animations_enabled; break;
        case SR_CLOCK_FORMAT:   s->clock_format =
                            (s->clock_format == CLOCK_FORMAT_24H)
                                ? CLOCK_FORMAT_12H : CLOCK_FORMAT_24H; break;
        case SR_CONFIRM_SHUTDOWN: s->confirm_shutdown = !s->confirm_shutdown; break;
        case SR_STORE_AUTO:     s->store_auto_update_index =
                                    !s->store_auto_update_index; break;
        default: return 0;   /* not adjustable */
    }
    settings_store_save(s);
    return 1;
}

static void settings_update(app *a, float dt_ms, const frame_input *in,
                            nav_request_t *out_nav)
{
    (void)dt_ms;

    if (in->back) { *out_nav = NAV_BACK; return; }

    if (in->up  && s_sel > 0)          { s_sel--; app_play_sound(a, "navigate"); }
    if (in->down && s_sel < SR_COUNT-1){ s_sel++; app_play_sound(a, "navigate"); }

    /* keep selection visible (rows shown depends on screen height) */
    int visible = (app_layout(a)->sh - 200) /
                  (app_theme(a)->sizes.title_size + 26);
    if (visible < 3) visible = 3;
    if (s_sel < s_scroll) s_scroll = s_sel;
    if (s_sel >= s_scroll + visible) s_scroll = s_sel - visible + 1;

    /* Inline adjustments for toggle/slider rows. */
    if (in->left)  row_adjust(a, s_sel, -1);
    if (in->right) row_adjust(a, s_sel, +1);

    if (in->confirm)
    {
        switch (s_sel)
        {
            case SR_THEME:    app_set_screen(a, SCREEN_THEME_SELECTOR);
                              app_play_sound(a, "select"); break;
            case SR_LANGUAGE: app_set_screen(a, SCREEN_LANGUAGE_SELECTOR);
                              app_play_sound(a, "select"); break;
            case SR_REMAP:    app_set_screen(a, SCREEN_INPUT_REMAP);
                              app_play_sound(a, "select"); break;
            default:          row_adjust(a, s_sel, +1);
                              app_play_sound(a, "select"); break;
        }
    }
}

static void settings_draw(app *a, float dt_ms)
{
    (void)dt_ms;
    discadia_theme *t = app_theme(a);
    layout *L = app_layout(a);
    widget_ctx w = app_widget_ctx(a);

    renderer_draw_background(app_renderer(a), t);
    top_bar_draw(a, locale_get("settings"));

    int row_h = t->sizes.title_size + 26;
    int x = L->content_left;
    int wp = L->content_right - L->content_left;
    int y0 = (int)L->icons_cy - (SR_COUNT * row_h) / 2;
    if (y0 < L->top_bar.y + L->top_bar.h + 10)
        y0 = L->top_bar.y + L->top_bar.h + 10;

    for (int i = s_scroll; i < SR_COUNT; i++)
    {
        if (i >= s_scroll + (L->sh - y0 - 60) / row_h) break;
        char val[64];
        value_for_row(a, i, val, sizeof(val));
        widget_list_row(&w, x, y0 + (i - s_scroll) * row_h, wp, row_h - 8,
                        locale_get(SR_LABEL_KEY[i]), val, i == s_sel);
    }
}

const screen_vtable *screen_settings_vtable(void)
{
    static const screen_vtable vt = {
        .name = "settings", .enter = settings_enter, .update = settings_update,
        .draw = settings_draw, .leave = settings_leave
    };
    return &vt;
}
