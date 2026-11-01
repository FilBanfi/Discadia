/*
 * empty_screen.c - friendly state shown by Home when the library has
 * no items: spinning disc glyph + localized guidance text.
 */

#include "ui/empty_screen.h"
#include "ui/widgets.h"
#include "i18n/locale.h"
#include "ui/animation.h"

#include <math.h>
#include <SDL.h>

void empty_screen_draw(app *a)
{
    layout *L = app_layout(a);
    discadia_theme *t = app_theme(a);
    widget_ctx w = app_widget_ctx(a);
    static float age_ms = 0.f;
    age_ms += 16.f;

    int cx = L->sw / 2, cy = L->icons_cy - L->icon_sel / 4;

    /* Spinning disc outline */
    float rot = anim_pingpong(age_ms, 4000.f) * 360.f - 180.f;
    SDL_Color acc = theme_color(t, TCOL_ACCENT);
    SDL_SetRenderDrawBlendMode(renderer_sdl(app_renderer(a)), SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_sdl(app_renderer(a)), acc.r, acc.g, acc.b, 190);
    for (int i = 0; i < 40; i++)
    {
        double ang = (i / 40.0) * 2 * M_PI + rot / 57.3;
        int r = L->icon_sel / 3;
        int x = cx + (int)(cos(ang) * r);
        int y = cy + (int)(sin(ang) * r * 0.6);
        SDL_RenderDrawPoint(renderer_sdl(app_renderer(a)), x, y);
    }

    int pulse = (int)(160 + 90 * anim_ease_out_quad(anim_pingpong(age_ms, 1600.f)));
    SDL_Color txt = theme_color(t, TCOL_TEXT);
    SDL_Color pt = { txt.r, txt.g, txt.b, (Uint8)pulse };
    widget_text(&w, FACE_TITLE, t->sizes.title_size, pt,
                locale_get("no_games"), cx, cy + L->icon_sel / 2, 0,
                L->sw - 2 * L->content_x, 1);
    widget_text_secondary(&w, FACE_BODY, t->sizes.description_size,
                          locale_get("insert_disc"), cx,
                          cy + L->icon_sel / 2 + t->sizes.title_size + 12, 0,
                          L->sw - 2 * L->content_x, 2);
}
