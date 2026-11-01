#ifndef DISCADIA_WIDGETS_H
#define DISCADIA_WIDGETS_H

/*
 * Reusable themed widgets shared by all screens: text drawing through
 * the font/texture caches, list rows, toggle/select rows, buttons and
 * procedural placeholder covers. All colors/sizes come from the theme.
 */

#include "ui/renderer.h"
#include "ui/font_cache.h"
#include "ui/texture_cache.h"
#include "theme/theme.h"

typedef struct widget_ctx {
    renderer       *ren;
    font_cache     *fcache;
    texture_cache  *tcache;
    discadia_theme *theme;
} widget_ctx;

/* Face names registered by app.c into the font cache. */
#define FACE_TITLE   "title"
#define FACE_BODY    "body"
#define FACE_SMALL   "small"

/* Draw single-line text (optionally wrapped/truncated via max_width and
 * max_lines). align: -1 left, 0 center, 1 right. y is the top edge.
 * Returns height drawn (0 on failure). */
int widget_text(widget_ctx *w, const char *face, int size, SDL_Color color,
               const char *utf8, int x_center_or_edge, int y, int align,
               int max_width, int max_lines);

/* Convenience wrappers using theme colors. */
int widget_text_primary(widget_ctx *w, const char *face, int size,
                        const char *utf8, int x, int y, int align,
                        int max_width, int max_lines);
int widget_text_secondary(widget_ctx *w, const char *face, int size,
                          const char *utf8, int x, int y, int align,
                          int max_width, int max_lines);

/* Procedural placeholder cover: rounded panel with the first letters of
 * the title (used when a game has no thumbnail). */
void widget_placeholder_cover(widget_ctx *w, int x, int y, int size,
                              const char *title);

/* Rounded icon panel with optional image; falls back to placeholder. */
void widget_cover(widget_ctx *w, int cx, int cy, int size,
                  const char *image_path, const char *fallback_title,
                  int selected);

/* A generic vertical list row: label left, value right, highlight bar
 * when selected. Row rect given by x,y,w,h. */
void widget_list_row(widget_ctx *w, int x, int y, int wpx, int h,
                     const char *label, const char *value, int selected);

/* Centered modal confirmation box with Yes/No. Draws over everything.
 * Returns nothing; selection state handled by caller screens. */
void widget_confirm_box(widget_ctx *w, int sw, int sh, const char *question,
                        int yes_selected);

#endif /* DISCADIA_WIDGETS_H */
