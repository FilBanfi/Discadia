/*
 * settings_screen.c - main settings page: a themed list whose entries
 * either toggle plain values (audio, animations, clock, confirm) or
 * open sub-screens (theme, language, input remap). Every change is
 * persisted immediately through settings_store_save(), so leaving the
 * screen with Back can never lose user data.
 */

#include "app/app.h"
#include "ui/widgets.h"
#include "ui/renderer.h"
#include "ui/top_bar.h"
#include "i18n/locale.h"
#include "settings/settings_store.h"
#include "audio/audio.h"
#include "log.h"

#include <string.h>

/* Row identifiers in display order. */
enum { ROW_THEME = 0, ROW_LANGUAGE, ROW_INPUT_REMAP, ROW_AUDIO, ROW_VOLUME,
       ROW_ANIMATIONS, ROW_CLOCK, ROW_CLOCK_FORMAT, ROW_CONFIRM_SHUTDOWN,
       ROW_BACK, ROW_COUNT };

static int s_selected;   /* focused row (kept across visits) */

/* i18n key for each row label. */
static const char *row_key(int row)
{
    switch (row)
    {
        case ROW_THEME:          return "theme";
        case ROW_LANGUAGE:       return "language";
        case ROW_INPUT_REMAP:    return "input_remap";
        case ROW_AUDIO:          return "audio_enabled";
        case ROW_VOLUME:         return "volume";
        case ROW_ANIMATIONS:     return "animations";
        case ROW_CLOCK:          return "show_clock";
        case ROW_CLOCK_FORMAT:   return "clock_format";
        case ROW_CONFIRM_SHUTDOWN: return "confirm_shutdown";
        default:                 return "back";
    }
}

/* Value label shown on the right side of each row. */
static const char *row_value(app *a, int row, char *buf, size_t bufsz)
{
    settings_model *s = app_settings(a);
    buf[0] = '\0';
    switch (row)
    {
        case ROW_THEME:     return s->theme;
        case ROW_LANGUAGE:  return locale_get_current();
        case ROW_INPUT_REMAP: return NULL;
        case ROW_AUDIO:     return locale_get(s->audio_enabled ? "on" : "off");
        case ROW_VOLUME:
            str_format(buf, bufsz, "%d%%", (int)(s->audio_volume * 100.f + 0.5f));
            return buf;
        case ROW_ANIMATIONS: return locale_get(s->animations_enabled ? "on" : "off");
        case ROW_CLOCK:      return locale_get(s->show_clock ? "on" : "off");
        case ROW_CLOCK_FORMAT:
            return locale_get(s->clock_format == CLOCK_FORMAT_12H
                                  ? "format_12h" : "format_24h");
        case ROW_CONFIRM_SHUTDOWN:
            return locale_get(s->confirm_shutdown ? "on" : "off");
        default:             return NULL;
    }
}

static void settings_enter(app *a) { (void)a; }
static void settings_leave(app *a) { (void)a; }

/* Apply + persist one logical change. */
static void commit(app *a)
{
    settings_store_save(app_settings(a));
}

static void settings_update(app *a, float dt_ms, const frame_input *in,
                            nav_request_t *out_nav)
{
    (void)dt_ms;
    settings_model *s = app_settings(a);

    if (in->up && s_selected > 0)
        { s_selected--; app_play_sound(a, "navigate"); }
    if (in->down && s_selected < ROW_COUNT - 1)
        { s_selected++; app_play_sound(a, "navigate"); }

    if (in->back) { *out_nav = NAV_BACK; return; }

    if (!in->confirm) return;

    switch (s_selected)
    {
        case ROW_THEME:
            app_play_sound(a, "select");
            app_set_screen(a, SCREEN_THEME_SELECTOR);
            break;
        case ROW_LANGUAGE:
            app_play_sound(a, "select");
            app_set_screen(a, SCREEN_LANGUAGE_SELECTOR);
            break;
        case ROW_INPUT_REMAP:
            app_play_sound(a, "select");
            app_set_screen(a, SCREEN_INPUT_REMAP);
            break;
        case ROW_AUDIO:
            s->audio_enabled = !s->audio_enabled;
            audio_set_enabled(app_audio_sys(a), s->audio_enabled);
            commit(a);
            app_play_sound(a, "select");
            break;
        case ROW_VOLUME:
            /* left/right adjust in 10% steps; confirm also nudges up */
            s->audio_volume += 0.1f;
            if (s->audio_volume > 1.001f) s->audio_volume = 0.f;
            if (s->audio_volume < 0.f) s->audio_volume = 1.f;
            audio_set_volume(app_audio_sys(a), s->audio_volume);
            commit(a);
            app_play_sound(a, "navigate");
            break;
        case ROW_ANIMATIONS:
            /* Global animation kill-switch honored by every tween. */
            s->animations_enabled = !s->animations_enabled;
            commit(a);
            app_play_sound(a, "select");
            break;
        case ROW_CLOCK:
            s->show_clock = !s->show_clock;
            commit(a);
            app_play_sound(a, "select");
            break;
        case ROW_CLOCK_FORMAT:
            s->clock_format = (s->clock_format == CLOCK_FORMAT_24H)
                                  ? CLOCK_FORMAT_12H : CLOCK_FORMAT_24H;
            commit(a);
            app_play_sound(a, "select");
            break;
        case ROW_CONFIRM_SHUTDOWN:
            s->confirm_shutdown = !s->confirm_shutdown;
            commit(a);
            app_play_sound(a, "select");
            break;
        case ROW_BACK:
            *out_nav = NAV_BACK;
            break;
    }
}

static void settings_draw(app *a, float dt_ms)
{
    discadia_theme *t = app_theme(a);
    layout *L = app_layout(a);
    widget_ctx w = app_widget_ctx(a);
    (void)dt_ms;

    renderer_draw_background(app_renderer(a), t);
    top_bar_draw(a, locale_get("settings"));

    int x = L->content_left;
    int wpx = L->content_right - L->content_left;
    int row_h = 52;
    int y = L->top_bar.h + 24;

    for (int r = 0; r < ROW_COUNT; r++)
    {
        char val[32];
        widget_list_row(&w, x, y + r * row_h, wpx, row_h - 6,
                        locale_get(row_key(r)), row_value(a, r, val, sizeof(val)),
                        r == s_selected);
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
