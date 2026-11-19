/*
 * input_remap_screen.c - rebind the 8 logical actions to controller
 * buttons. Flow: focus a row, press Confirm -> "waiting for input",
 * press any button -> bound via input_mapping_set() and saved. Back
 * cancels the capture; a final Back leaves the screen. Keyboard
 * fallbacks stay active in parallel (see input_mapping.h), so a bad
 * gamepad binding can never lock the user out.
 */

#include "app/app.h"
#include "ui/widgets.h"
#include "ui/renderer.h"
#include "ui/top_bar.h"
#include "i18n/locale.h"
#include "input/input.h"
#include "input/input_mapping.h"
#include "log.h"

static int s_selected;
static int s_capturing;    /* 1 = waiting for a physical button */

static void ir_enter(app *a) { (void)a; s_capturing = 0; }
static void ir_leave(app *a) { s_capturing = 0; input_capture_end(); }

static void ir_update(app *a, float dt_ms, const frame_input *in,
                      nav_request_t *out_nav)
{
    (void)dt_ms;

    if (s_capturing)
    {
        /* The input layer records the first physical press while capture
         * mode is on; poll it once per frame (NULL until something hits). */
        const char *btn = input_capture_next_button();
        if (btn)
        {
            if (strcmp(btn, "BACK") == 0 || strcmp(btn, "SELECT") == 0 ||
                strcmp(btn, "KEY_ESCAPE") == 0)
            {
                s_capturing = 0;                        /* user cancelled */
                app_play_sound(a, "back");
            }
            else if (input_mapping_set((input_action_t)s_selected, btn) == 0)
            {
                input_mapping_save();
                app_play_sound(a, "select");
                s_capturing = 0;
            }
        }
        return;   /* all other input ignored while capturing */
    }

    if (in->up && s_selected > 0)
        { s_selected--; app_play_sound(a, "navigate"); }
    if (in->down && s_selected < IA_COUNT - 1)
        { s_selected++; app_play_sound(a, "navigate"); }

    if (in->confirm)
    {
        s_capturing = 1;
        input_capture_begin();
        app_play_sound(a, "select");
    }
    else if (in->back)
    {
        *out_nav = NAV_BACK;
    }
}

static void ir_draw(app *a, float dt_ms)
{
    discadia_theme *t = app_theme(a);
    layout *L = app_layout(a);
    widget_ctx w = app_widget_ctx(a);
    (void)dt_ms;

    renderer_draw_background(app_renderer(a), t);
    top_bar_draw(a, locale_get("input_remap"));

    int x = L->content_left;
    int wpx = L->content_right - L->content_left;
    int row_h = (L->sh - L->top_bar.h - 60) / IA_COUNT;
    if (row_h > 64) row_h = 64;
    int y0 = L->top_bar.h + 24;

    for (int i = 0; i < IA_COUNT; i++)
    {
        const char *value = input_mapping_get((input_action_t)i);
        if (s_capturing && i == s_selected)
            value = locale_get("waiting_input");   /* pulsing hint text */
        widget_list_row(&w, x, y0 + i * row_h, wpx, row_h - 6,
                        locale_get(input_mapping_action_key((input_action_t)i)),
                        value, i == s_selected);
    }
}

const screen_vtable *screen_input_remap_vtable(void)
{
    static const screen_vtable vt = {
        .name = "input_remap", .enter = ir_enter, .update = ir_update,
        .draw = ir_draw, .leave = ir_leave
    };
    return &vt;
}
