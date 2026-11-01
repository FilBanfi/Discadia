/*
 * top_bar.c - the fixed-height bar at the top of every screen:
 * title (selected game or app name), optional gear hint and clock.
 */

#include "ui/top_bar.h"
#include "ui/widgets.h"
#include "i18n/locale.h"
#include "system/time_service.h"
#include "util/string_util.h"

#include <string.h>

void top_bar_draw(app *a, const char *title)
{
    layout *L = app_layout(a);
    discadia_theme *t = app_theme(a);
    settings_model *s = app_settings(a);
    widget_ctx w = app_widget_ctx(a);

    /* Bar background strip in secondary color, subtle. */
    SDL_Color bg = theme_color(t, TCOL_BACKGROUND_GRADIENT_END);
    renderer_fill_rounded(app_renderer(a), 0, 0, L->sw, L->top_bar.h,
                          0, bg, 60);

    /* Title left-aligned with padding. */
    if (!title || !*title) title = locale_get("app_title");
    widget_text_primary(&w, FACE_TITLE, t->sizes.title_size, title,
                        L->content_left, L->top_bar.y + 8, -1,
                        L->sw / 2, 1);

    /* Clock right-aligned when enabled. */
    if (s && s->show_clock)
    {
        char clk[32];
        time_now_string(clk, sizeof(clk),
                        s->clock_format == CLOCK_FORMAT_12H ? TS_12H : TS_24H);
        widget_text_secondary(&w, FACE_SMALL, t->sizes.meta_size, clk,
                              L->content_right, L->top_bar.y + 16, 1, 200, 1);
    }
}
