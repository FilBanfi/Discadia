#ifndef DISCADIA_TEXTURE_CACHE_H
#define DISCADIA_TEXTURE_CACHE_H

/*
 * Texture cache: bounded LRU store of SDL textures created from image
 * files (SDL2_image) or from font surfaces. Covers, icons and text
 * bitmaps all flow through here so each file/surface is uploaded once.
 * When the cache exceeds TC_MAX_TEXTURES entries the least recently
 * used texture is destroyed - important on 512MB devices with large
 * libraries.
 */

#include <SDL.h>

typedef struct texture_cache texture_cache;

texture_cache *texture_cache_create(SDL_Renderer *renderer);
void           texture_cache_destroy(texture_cache *tc);

/* Load an image file into a cached streaming texture. Returns borrowed
 * pointer owned by the cache, NULL on failure. */
SDL_Texture *texture_cache_load_file(texture_cache *tc, const char *path);

/* Upload (or reuse) a surface under the given key. The surface is NOT
 * copied after upload and must stay valid only during this call. */
SDL_Texture *texture_cache_from_surface(texture_cache *tc, const char *key,
                                        SDL_Surface *surface);

/* Draw a cached file texture scaled to fit dst preserving aspect ratio,
 * centered inside dst. out_rect receives the final rect (may be NULL). */
SDL_Texture *texture_cache_draw_fit(texture_cache *tc, const char *path,
                                    SDL_Rect dst, SDL_Rect *out_rect);

/* Number of currently cached textures (for debug/stats). */
int texture_cache_count(texture_cache *tc);

#endif /* DISCADIA_TEXTURE_CACHE_H */
