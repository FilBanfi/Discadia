/*
 * carousel.c - horizontal game strip with animated scrolling.
 *
 * The animated position (app_carousel_pos, a float index) eases toward
 * the selected index using theme.animations.scroll_speed_ms as the time
 * constant; the selected icon scales up with selection_scale_ms. Icons
 * are drawn centered on screen so the strip scrolls beneath them.
 */

#include "ui/carousel.h"
#include "ui/widgets.h"
#include "ui/animation.h"
#include "library/library.h"
#include "log.h"

#include <SDL.h>
#include <math.h>
#include <string.h>

void carousel_move(app *a, int delta)
{
    int n = library_count(app_library(a));
    if (n <= 0) return;
    int sel = app_selected_item(a);
    /* wraparound like classic console carousels */
    sel = ((sel + delta) % n + n) % n;
    app_set_selected(a, sel);
    app_play_sound(a, "navigate");
}

void carousel_jump(app *a, int to_last)
{
    int n = library_count(app_library(a));
    if (n <= 0) return;
    app_set_selected(a, to_last ? n - 1 : 0);
    app_play_sound(a, "navigate");
}

void carousel_update(app *a, float dt_ms, const frame_input *in, int *nav_out)
{
    (void)nav_out;
    discadia_theme *t = app_theme(a);
    float target = (float)app_selected_item(a);
    float pos = app_carousel_pos(a);

    if (t->animations.enabled)
    {
        float tau = (float)(t->animations.scroll_speed_ms > 0
                                ? t->animations.scroll_speed_ms : 220);
        /* shortest-path smoothing across wrap: keep pos continuous */
        pos = anim_approach(pos, target, dt_ms, tau);
        if (fabsf(pos - target) < 0.002f) pos = target;
    }
    else
    {
        pos = target;   /* animations off: instant jumps everywhere */
    }
    app_set_carousel_pos(a, pos);

    if (in->left)  carousel_move(a, -1);
    if (in->right) carousel_move(a, +1);
    if (in->context)
    {
        /* context toggles jump-to-last / jump-to-first feel */
        int n = library_count(app_library(a));
        carousel_jump(a, app_selected_item(a) != (n > 0 ? n - 1 : 0));
    }
}

void carousel_draw(app *a, float dt_ms)
{
    layout *L = app_layout(a);
    discadia_theme *t = app_theme(a);
    widget_ctx w = app_widget_ctx(a);
    library *lib = app_library(a);
    int n = library_count(lib);
    if (n <= 0) return;

    float pos = app_carousel_pos(a);
    int cx_screen = L->sw / 2;
    int step = L->icon_spacing > 0 ? L->icon_spacing : L->icon_normal + 24;

    /* how many icons can be visible at once (both sides + center) */
    int reach = cx_screen / (step / 2) + 2;
    int center = (int)lroundf(pos);

    for (int i = center - reach; i <= center + reach; i++)
    {
        int idx = ((i % n) + n) % n;          /* wraparound */
        const library_item *it = library_get(lib, idx);
        if (!it) continue;

        float d = (float)i - pos;             /* signed distance */
        if (d < -(float)n / 2.f) d += (float)n;
        if (d >  (float)n / 2.f) d -= (float)n;

        int is_sel = (idx == app_selected_item(a));

        /* scale factor eased from selection proximity */
        float prox = 1.f - anim_clamp(fabsf(d), 0.f, 1.f);
        float scale = 0.75f + 0.25f * anim_ease_smooth(prox);
        if (is_sel && t->animations.enabled)
        {
            float p = anim_clamp(dt_ms /
                        (float)(t->animations.selection_scale_ms > 0
                                    ? t->animations.selection_scale_ms : 160),
                        0.f, 1.f);
            scale *= 1.f + 0.06f * p;         /* subtle pop on change */
        }
        else if (is_sel) scale *= 1.06f;

        int size = (int)((is_sel ? L->icon_selected : L->icon_normal) * scale);
        int x = cx_screen + (int)(d * step);
        int y = (int)L->icons_cy;

        if (x + size / 2 < -size || x - size / 2 > L->sw + size) continue;

        char thumb[768];
        thumb[0] = '\0';
        if (it->thumbnail[0])
        {
            snprintf(thumb, sizeof(thumb), "%s/%s",
                     it->metadata_dir, it->thumbnail);
        }
        widget_cover(&w, x, y, size, thumb, it->title, is_sel);
    }
}
