/*
 * layout.c - see layout.h
 */

#include "ui/layout.h"
#include "theme/theme.h"

void layout_compute(layout *L, const discadia_theme *t, int sw, int sh)
{
    if (!L || !t) return;
    L->sw = sw;
    L->sh = sh;

    int pad = t->layout.top_bar_padding;
    if (pad < 8) pad = 8;
    if (pad > sw / 4) pad = sw / 4;

    L->top_bar = (SDL_Rect){ 0, 0, sw, t->sizes.top_bar_height };
    L->content_left = pad;
    L->content_right = sw - pad;

    /* Scale icon metrics down on very small screens so at least three
     * covers stay visible (Pi Zero @ 640x480). */
    int in = t->sizes.icon_normal, isel = t->sizes.icon_selected;
    int sp = t->sizes.icon_spacing;
    while (sp > 60 && 3 * sp > sw) { in = in * 9 / 10; isel = isel * 9 / 10; sp = sp * 9 / 10; }
    L->icon_normal = in;
    L->icon_selected = isel > in ? isel : in + in / 4;
    L->icon_spacing = sp;

    L->icons_cy = (float)(sh * t->layout.icons_y);

    int desc_h = t->sizes.description_size * 4;   /* up to 4 wrapped lines */
    int meta_h = t->sizes.meta_size * 2;
    L->description_y = (int)(sh * t->layout.description_y);
    L->meta_y = (int)(sh * t->layout.meta_y);
    if (L->meta_y < L->description_y + desc_h) L->meta_y = L->description_y + desc_h;

    int panel_top = L->description_y - t->sizes.description_size;
    int panel_bot = L->meta_y + meta_h;
    if (panel_bot > sh) panel_bot = sh;
    L->metadata_panel = (SDL_Rect){ pad, panel_top, sw - 2 * pad, panel_bot - panel_top };
}
