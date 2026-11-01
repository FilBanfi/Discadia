#ifndef DISCADIA_FONT_CACHE_H
#define DISCADIA_FONT_CACHE_H

/*
 * Font cache: opens TTF faces once (theme fonts with system fallback)
 * and caches rendered text surfaces keyed by face+size+text. Surfaces
 * are turned into textures lazily by the texture helper in widgets.c.
 * LRU eviction keeps memory bounded on 512MB devices.
 */

#include <SDL.h>
#include <SDL_ttf.h>

typedef struct font_cache font_cache;

font_cache *font_cache_create(void);
void        font_cache_destroy(font_cache *fc);

/* Register a named face from a path; returns 0 on success. The cache
 * keeps its own TTF_Font handle. `fallback` is tried when path is empty
 * or fails to load (pass NULL for none). */
int font_cache_add_face(font_cache *fc, const char *name,
                        const char *path, int size, const char *fallback_path);

/* Render UTF-8 text with the given face/size/color into a cached surface.
 * Returns borrowed pointer (owned by cache), NULL on failure. max_width>0
 * wraps words; lines>0 limits wrapped line count (last line ellipsized). */
SDL_Surface *font_cache_text(font_cache *fc, const char *face, int size,
                             SDL_Color color, const char *utf8,
                             int max_width, int max_lines);

/* Pixel width of the text as it would render single-line. */
int font_cache_measure(font_cache *fc, const char *face, int size,
                       const char *utf8);

/* Drop all cached surfaces (call after theme change). Faces stay open. */
void font_cache_flush(font_cache *fc);

#endif /* DISCADIA_FONT_CACHE_H */
