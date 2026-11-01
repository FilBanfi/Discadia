#ifndef DISCADIA_RENDERER_H
#define DISCADIA_RENDERER_H

/*
 * Thin wrapper over SDL_Renderer + window. Owns logical screen size and
 * a small set of primitives (filled rounded rects, soft glow, gradient
 * background) used by every screen. All drawing reads colors from the
 * theme - never hardcode them here.
 */

#include <SDL.h>
#include "theme/theme.h"

typedef struct renderer renderer;

/* Create fullscreen window + renderer. video_driver may be NULL ("auto").
 * Returns NULL on failure with reason logged. */
renderer *renderer_create(int fullscreen, const char *video_driver);
void      renderer_destroy(renderer *r);

SDL_Renderer *renderer_sdl(renderer *r);
void          renderer_size(const renderer *r, int *w, int *h);

/* Clear with vertical gradient background: primary top color to
 * gradient_end bottom color (both from theme). */
void renderer_draw_background(renderer *r, const discadia_theme *t);

/* Rounded filled rectangle with optional alpha. */
void renderer_fill_rounded(renderer *r, int x, int y, int w, int h,
                           int radius, SDL_Color color, int alpha);

/* Rounded outline only. */
void renderer_outline_rounded(renderer *r, int x, int y, int w, int h,
                              int radius, SDL_Color color, int thickness);

/* Soft rectangular glow ring (layered translucent outlines), used for
 * the selected carousel item. */
void renderer_glow_rect(renderer *r, int x, int y, int w, int h,
                        int radius, SDL_Color color, int layers);

/* Horizontal thin line. */
void renderer_hline(renderer *r, int x, int y, int w, SDL_Color color, int alpha);

/* Scanline overlay effect (subtle dark lines every `spacing` rows). */
void renderer_scanlines(renderer *r, int spacing, int alpha);

/* Present frame. */
void renderer_present(renderer *r);

/* Debug helpers (used when DISCADIA_DEBUG=1). */
void renderer_debug_rect(renderer *r, int x, int y, int w, int h);

#endif /* DISCADIA_RENDERER_H */
