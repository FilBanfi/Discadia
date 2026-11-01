/*
 * renderer.c - window/renderer wrapper + themed primitives.
 * Rounded-rect fill is implemented as horizontal spans (cheap, no extra
 * libs) which looks smooth enough at our resolutions and works on every
 * SDL render backend including software fallback.
 */

#include "ui/renderer.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define GRAD_STEP_PX 2   /* gradient band height in pixels */

struct renderer {
    SDL_Window   *window;
    SDL_Renderer *sdl;
    int width, height;
};

renderer *renderer_create(int fullscreen, const char *video_driver)
{
    if (video_driver && *video_driver)
        SDL_SetHint(SDL_HINT_VIDEODRIVER, video_driver);

    if (!SDL_WasInit(SDL_INIT_VIDEO))
    {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
        {
            LOG_ERROR("SDL_InitSubSystem(VIDEO) failed: %s", SDL_GetError());
            return NULL;
        }
    }

    renderer *r = calloc(1, sizeof(*r));
    if (!r) return NULL;

    Uint32 flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
    int w = 0, h = 0;

    /* Discover current desktop/display mode for a sensible size. */
    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0) { w = dm.w; h = dm.h; }
    if (w <= 0 || h <= 0) { w = 1280; h = 720; }

    if (fullscreen) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    r->window = SDL_CreateWindow("Discadia", 0, 0, w, h, flags);
    if (!r->window)
    {
        LOG_WARN("fullscreen window failed (%s), retrying windowed",
                 SDL_GetError());
        r->window = SDL_CreateWindow("Discadia", SDL_WINDOWPOS_CENTERED,
                                     SDL_WINDOWPOS_CENTERED, w, h,
                                     SDL_WINDOW_RESIZABLE);
    }
    if (!r->window)
    {
        LOG_ERROR("cannot create window: %s", SDL_GetError());
        free(r);
        return NULL;
    }

    r->sdl = SDL_CreateRenderer(r->window, -1, flags);
    if (!r->sdl)
        r->sdl = SDL_CreateRenderer(r->window, -1, SDL_RENDERER_SOFTWARE);
    if (!r->sdl)
    {
        LOG_ERROR("cannot create renderer: %s", SDL_GetError());
        SDL_DestroyWindow(r->window);
        free(r);
        return NULL;
    }

    SDL_SetRenderDrawBlendMode(r->sdl, SDL_BLENDMODE_BLEND);
    SDL_GetRendererOutputSize(r->sdl, &r->width, &r->height);
    LOG_INFO("renderer ready: %dx%d (%s)", r->width, r->height,
             SDL_GetCurrentVideoDriver());
    return r;
}

void renderer_destroy(renderer *r)
{
    if (!r) return;
    if (r->sdl)   SDL_DestroyRenderer(r->sdl);
    if (r->window) SDL_DestroyWindow(r->window);
    free(r);
}

SDL_Renderer *renderer_sdl(renderer *r) { return r ? r->sdl : NULL; }

void renderer_size(const renderer *r, int *w, int *h)
{
    if (w) *w = r ? r->width : 0;
    if (h) *h = r ? r->height : 0;
}

void renderer_draw_background(renderer *r, const discadia_theme *t)
{
    if (!r || !r->sdl || !t) return;
    SDL_Color a = theme_color(t, TCOL_BACKGROUND);
    SDL_Color b = theme_color(t, TCOL_BACKGROUND_GRADIENT_END);

    SDL_SetRenderDrawColor(r->sdl, a.r, a.g, a.b, 255);
    SDL_RenderClear(r->sdl);

    for (int y = 0; y < r->height; y += GRAD_STEP_PX)
    {
        float k = (float)y / (float)(r->height > 1 ? r->height - 1 : 1);
        SDL_Color c = {
            (Uint8)(a.r + (b.r - a.r) * k),
            (Uint8)(a.g + (b.g - a.g) * k),
            (Uint8)(a.b + (b.b - a.b) * k),
            255
        };
        SDL_SetRenderDrawColor(r->sdl, c.r, c.g, c.b, c.a);
        SDL_Rect band = { 0, y, r->width, GRAD_STEP_PX };
        SDL_RenderFillRect(r->sdl, &band);
    }
}

/* Distance-based corner test for one row of a rounded rect. Returns the
 * inset needed at that row so corners look circular. */
static int corner_inset(int radius, int dy)
{
    if (radius <= 0 || dy >= radius) return 0;
    double rr = (double)radius;
    double dd = (double)(radius - dy);      /* distance from center axis */
    double cut = rr - sqrt(rr * rr - dd * dd);
    if (cut < 0) cut = 0;
    return (int)(cut + 0.5);
}

void renderer_fill_rounded(renderer *r, int x, int y, int w, int h,
                           int radius, SDL_Color color, int alpha)
{
    if (!r || !r->sdl || w <= 0 || h <= 0) return;
    if (radius > w / 2) radius = w / 2;
    if (radius > h / 2) radius = h / 2;

    SDL_Color c = color; c.a = (Uint8)alpha;
    SDL_SetRenderDrawColor(r->sdl, c.r, c.g, c.b, c.a);

    for (int row = 0; row < h; row++)
    {
        int inset = 0;
        if (row < radius)                inset = corner_inset(radius, row);
        else if (row >= h - radius)      inset = corner_inset(radius, h - 1 - row);
        if (inset <= 0)
        {
            SDL_Rect line = { x, y + row, w, 1 };
            SDL_RenderFillRect(r->sdl, &line);
        }
        else if (w - 2 * inset > 0)
        {
            SDL_Rect line = { x + inset, y + row, w - 2 * inset, 1 };
            SDL_RenderFillRect(r->sdl, &line);
        }
    }
}

void renderer_outline_rounded(renderer *r, int x, int y, int w, int h,
                              int radius, SDL_Color color, int thickness)
{
    if (!r || !r->sdl || w <= 0 || h <= 0) return;
    if (thickness < 1) thickness = 1;
    SDL_SetRenderDrawColor(r->sdl, color.r, color.g, color.b, color.a);

    /* Draw `thickness` concentric rounded outlines by shrinking. */
    for (int t = 0; t < thickness; t++)
    {
        int rx = x + t, ry = y + t, rw = w - 2 * t, rh = h - 2 * t;
        if (rw <= 0 || rh <= 0) break;
        int rad = radius - t; if (rad < 0) rad = 0;
        if (rad == 0)
        {
            SDL_Rect top = { rx, ry, rw, 1 };
            SDL_Rect bot = { rx, ry + rh - 1, rw, 1 };
            SDL_Rect lft = { rx, ry, 1, rh };
            SDL_Rect rgt = { rx + rw - 1, ry, 1, rh };
            SDL_RenderFillRect(r->sdl, &top);
            SDL_RenderFillRect(r->sdl, &bot);
            SDL_RenderFillRect(r->sdl, &lft);
            SDL_RenderFillRect(r->sdl, &rgt);
            continue;
        }
        for (int row = 0; row < rh; row++)
        {
            int inset = 0;
            if (row < rad)               inset = corner_inset(rad, row);
            else if (row >= rh - rad)    inset = corner_inset(rad, rh - 1 - row);
            int px = rx + inset;
            int pw = rw - 2 * inset;
            if (pw <= 0) continue;
            SDL_Rect a = { px, ry + row, 1, 1 };
            SDL_Rect b = { px + pw - 1, ry + row, 1, 1 };
            SDL_RenderFillRect(r->sdl, &a);
            SDL_RenderFillRect(r->sdl, &b);
        }
    }
}

void renderer_glow_rect(renderer *r, int x, int y, int w, int h,
                        int radius, SDL_Color color, int layers)
{
    if (!r || !r->sdl || layers <= 0) return;
    for (int i = 0; i < layers; i++)
    {
        int grow = i + 1;
        int alpha = 200 / (grow + 1);
        SDL_Color c = color; c.a = (Uint8)alpha;
        renderer_outline_rounded(r, x - grow, y - grow,
                                 w + 2 * grow, h + 2 * grow,
                                 radius + grow, c, 1);
    }
}

void renderer_hline(renderer *r, int x, int y, int w, SDL_Color color, int alpha)
{
    if (!r || !r->sdl || w <= 0) return;
    SDL_Color c = color; c.a = (Uint8)alpha;
    SDL_SetRenderDrawColor(r->sdl, c.r, c.g, c.b, c.a);
    SDL_Rect line = { x, y, w, 1 };
    SDL_RenderFillRect(r->sdl, &line);
}

void renderer_scanlines(renderer *r, int spacing, int alpha)
{
    if (!r || !r->sdl || spacing <= 0) return;
    SDL_SetRenderDrawColor(r->sdl, 0, 0, 0, (Uint8)alpha);
    for (int y = 0; y < r->height; y += spacing)
    {
        SDL_Rect line = { 0, y, r->width, 1 };
        SDL_RenderFillRect(r->sdl, &line);
    }
}

void renderer_present(renderer *r)
{
    if (r && r->sdl) SDL_RenderPresent(r->sdl);
}

void renderer_debug_rect(renderer *r, int x, int y, int w, int h)
{
    if (!r || !discadia_debug_enabled()) return;
    SDL_Color c = { 0x00, 0xff, 0x00, 0x60 };
    renderer_outline_rounded(r, x, y, w, h, 0, c, 1);
}
