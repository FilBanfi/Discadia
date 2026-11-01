#ifndef DISCADIA_LAYOUT_H
#define DISCADIA_LAYOUT_H

/*
 * Layout: converts the theme's relative positions/sizes into absolute
 * pixel rects for the current screen size. Every screen asks layout for
 * its regions instead of computing coordinates itself, so changing the
 * theme (or resolution) moves everything coherently.
 */

#include <SDL.h>
#include "theme/theme.h"

typedef struct layout {
    int sw, sh;                 /* screen size                     */
    SDL_Rect top_bar;           /* fixed header                    */
    float icons_cy;             /* carousel center y (px)          */
    SDL_Rect metadata_panel;    /* bottom panel region             */
    int description_y;          /* top of description text         */
    int meta_y;                 /* top of metadata line            */
    int content_left;           /* safe left padding               */
    int content_right;          /* screen width - right padding    */
    int icon_normal;            /* scaled from theme sizes         */
    int icon_selected;
    int icon_spacing;
} layout;

/* Recompute all regions for a screen size using theme values. */
void layout_compute(layout *out, const discadia_theme *t, int sw, int sh);

#endif /* DISCADIA_LAYOUT_H */
