/*
 * input_remap_screen.c - rebind the eight logical actions.
 *
 * Flow: select an action, press CONFIRM to enter "listening" mode, then
 * press any gamepad button / keyboard key. The event is translated via
 * input_event_to_button_name(), stored in the mapping and persisted
 * atomically to ~/.config/discadia/input-map.json. BACK cancels while
 * listening; fixed keys (Q / Space) are never captured so the user can
 * always escape.
 */

#include <string.h>
#include "ui/top_bar.h"
#include <SDL.h>
#include "app/app.h"
#include "audio/audio.h"
#include "input/input.h"
#include "input/input_mapping.h"
#include "settings/settings_store.h"
#include "ui/widgets.h"
#include "ui/renderer.h"
#include "ui/toast.h"
#include "i18n/locale.h"
#include "util/string_util.h"
#include "log.h"

static int s_sel = 0;        /* selected action row            */
static int s_listening = -1; /* action index being rebound     */
static int s_listen_frames = 0;        /* capture timeout counter    */

static void ir_enter(app *a)
{
    (void)a;
    s_sel = 0;
    s_listening = -1;
}

static void ir_leave(app *a) { (void)a; }

/* Consume SDL events until a mappable button is captured. Returns the
 * button name or NULL when nothing suitable arrived this frame. */
static const char *poll_capture(void)
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
    {
        if (ev.type == SDL_QUIT) return NULL;
        const char *name = input_event_to_button_name(&ev);
        if (!name) continue;
        /* Never steal the fixed escape keys. */
        if (!strcmp(name, FIXED_QUIT_KEY) ||
            !strcmp(name, FIXED_CONTEXT_KEY)) continue;
        return name;
    }
    return NULL;
}

static void ir_update(app *a, float dt_ms, const frame_input *in,
                      nav_request_t *out_nav)
{
    (void)dt_ms;

    if (s_listening >= 0)
    {
        /* Modal capture state: BACK cancels, any button rebinds. */
        if (in->back)
        {
            s_listening = -1;
            app_play_sound(a, "back");
            return;
        }
        const char *btn = poll_capture();
        if (btn)
        {
            if (input_mapping_set((input_action_t)s_listening, btn) == 0)
            {
                input_mapping_save();
                char msg[256];
                str_format(msg, sizeof(msg), "%s: %s -> %s",
                           locale_get(input_mapping_action_key(
                               (input_action_t)s_listening)),
                           btn, locale_get("update_complete"));
                toast_push(app_toasts(a), msg);
                app_play_sound(a, "select");
            }
            s_listening = -1;
        }
        /* Safety timeout so the user can never be trapped in capture. */
        else if (++s_listen_frames > 60 * 10)   /* ~10 seconds */
        {
            s_listening = -1;
            toast_push(app_toasts(a), locale_get("remap_timeout"));
        }
        return;
    }

    if (in->back) { *out_nav = NAV_BACK; return; }
    if (in->up && s_sel > 0)              { s_sel--; app_play_sound(a, "navigate"); }
    if (in->down && s_sel < IA_COUNT - 1) { s_sel++; app_play_sound(a, "navigate"); }
    if (in->confirm)
    {
        s_listening = s_sel;
        s_listen_frames = 0;
        app_play_sound(a, "select");
    }
}

static void ir_draw(app *a, float dt_ms)
{
    discadia_theme *t = app_theme(a);
    layout *L = app_layout(a);
    widget_ctx w = app_widget_ctx(a);

    renderer_draw_background(app_renderer(a), t);
    top_bar_draw(a, locale_get("settings_input_remap"));

    int row_h = t->sizes.title_size + 26;
    int y0 = L->top_bar.y + L->top_bar.h + 20;
    int x = L->content_left, wp = L->content_right - L->content_left;

    for (int i = 0; i < IA_COUNT; i++)
    {
        const char *val = input_mapping_get((input_action_t)i);
        char buf[64];
        if (s_listening == i)
        {
            /* Blinking prompt while capturing. */
            str_safe_copy(buf, locale_get("remap_prompt"), sizeof(buf));
            if (((int)(dt_ms / 300)) % 2 == 0)
                widget_list_row(&w, x, y0 + i * row_h, wp, row_h - 8,
                                locale_get(input_mapping_action_key(i)),
                                "", 1);
            else
                widget_list_row(&w, x, y0 + i * row_h, wp, row_h - 8,
                                locale_get(input_mapping_action_key(i)),
                                buf, 1);
        }
        else
        {
            widget_list_row(&w, x, y0 + i * row_h, wp, row_h - 8,
                            locale_get(input_mapping_action_key(i)),
                            val ? val : "?", i == s_sel);
        }
    }

    widget_text_secondary(&w, FACE_SMALL, t->sizes.meta_size,
                          locale_get("press_back_hint"), x, L->sh - 40, -1,
                          wp, 1);
}

const screen_vtable *screen_input_remap_vtable(void)
{
    static const screen_vtable vt = {
        .name = "input_remap", .enter = ir_enter, .update = ir_update,
        .draw = ir_draw, .leave = ir_leave
    };
    return &vt;
}
