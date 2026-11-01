/*
 * texture_cache.c - bounded LRU of SDL textures.
 * Keys are file paths or caller-provided strings for surfaces. Failed
 * loads are remembered briefly so a missing cover does not hit the disk
 * every frame (negative caching with TC_MAX_MISSING entries).
 */

#include "ui/texture_cache.h"
#include "util/string_util.h"
#include "log.h"

#include <SDL_image.h>
#include <stdlib.h>
#include <string.h>

#define TC_MAX_TEXTURES 64
#define TC_MAX_MISSING  128

typedef struct tc_entry {
    struct tc_entry *next;
    char      *key;
    SDL_Texture *tex;          /* NULL => negative cache entry */
} tc_entry;

struct texture_cache {
    SDL_Renderer *renderer;
    tc_entry     *head;        /* MRU order */
    int           count;       /* real textures */
    int           missing;     /* negative entries */
};

texture_cache *texture_cache_create(SDL_Renderer *renderer)
{
    if (!renderer) return NULL;
    texture_cache *tc = calloc(1, sizeof(*tc));
    if (tc) tc->renderer = renderer;
    return tc;
}

static void entry_free(tc_entry *e)
{
    if (!e) return;
    if (e->tex) SDL_DestroyTexture(e->tex);
    free(e->key);
    free(e);
}

void texture_cache_destroy(texture_cache *tc)
{
    if (!tc) return;
    tc_entry *e = tc->head;
    while (e) { tc_entry *n = e->next; entry_free(e); e = n; }
    free(tc);
}

int texture_cache_count(texture_cache *tc) { return tc ? tc->count : 0; }

static tc_entry *find_move_mru(texture_cache *tc, const char *key)
{
    tc_entry *prev = NULL, *cur = tc->head;
    while (cur)
    {
        if (strcmp(cur->key, key) == 0)
        {
            if (prev) { prev->next = cur->next; cur->next = tc->head; tc->head = cur; }
            return cur;
        }
        prev = cur; cur = cur->next;
    }
    return NULL;
}

static void evict_lru_real(texture_cache *tc)
{
    /* Evict from the tail, skipping negative entries when possible. */
    while (tc->count > TC_MAX_TEXTURES)
    {
        tc_entry *prev = NULL, *cur = tc->head;
        tc_entry *last_real_prev = NULL, *last_real = NULL;
        while (cur)
        {
            if (cur->tex) { last_real_prev = prev; last_real = cur; }
            prev = cur; cur = cur->next;
        }
        if (!last_real) break;
        if (last_real_prev) last_real_prev->next = last_real->next;
        else tc->head = last_real->next;
        entry_free(last_real);
        tc->count--;
    }
}

static void evict_missing(texture_cache *tc)
{
    while (tc->missing > TC_MAX_MISSING)
    {
        tc_entry *prev = NULL, *cur = tc->head;
        tc_entry *last_prev = NULL, *last = NULL;
        while (cur)
        {
            if (!cur->tex) { last_prev = prev; last = cur; }
            prev = cur; cur = cur->next;
        }
        if (!last) break;
        if (last_prev) last_prev->next = last->next;
        else tc->head = last->next;
        entry_free(last);
        tc->missing--;
    }
}

static tc_entry *insert(texture_cache *tc, const char *key, SDL_Texture *tex)
{
    tc_entry *e = calloc(1, sizeof(*e));
    if (!e) return NULL;
    e->key = strdup(key);
    e->tex = tex;
    if (!e->key) { free(e); if (tex) SDL_DestroyTexture(tex); return NULL; }
    e->next = tc->head;
    tc->head = e;
    if (tex) tc->count++; else tc->missing++;
    evict_lru_real(tc);
    evict_missing(tc);
    return e;
}

SDL_Texture *texture_cache_load_file(texture_cache *tc, const char *path)
{
    if (!tc || !path || !*path) return NULL;

    tc_entry *hit = find_move_mru(tc, path);
    if (hit) return hit->tex;   /* may be NULL: negative cache */

    SDL_Surface *surf = IMG_Load(path);
    if (!surf)
    {
        LOG_DEBUG("texture load failed (%s): %s", path, IMG_GetError());
        insert(tc, path, NULL);   /* remember the miss */
        return NULL;
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(tc->renderer, surf);
    SDL_FreeSurface(surf);
    if (!tex)
    {
        LOG_WARN("texture upload failed for %s: %s", path, SDL_GetError());
        insert(tc, path, NULL);
        return NULL;
    }
    tc_entry *e = insert(tc, path, tex);
    return e ? e->tex : tex;
}

/* Build a stable key for surface uploads: face|size|hash(text)|w|h. */
SDL_Texture *texture_cache_from_surface(texture_cache *tc, const char *key,
                                       SDL_Surface *surface)
{
    if (!tc || !key || !surface) return NULL;
    char full[512];
    str_format(full, sizeof(full), "%s|%dx%d#%p", key,
               surface->w, surface->h, (void *)surface);
    /* The pointer in the key ties the texture to this exact surface;
     * font cache owns surfaces so they stay valid while cached. */
    tc_entry *hit = find_move_mru(tc, full);
    if (hit) return hit->tex;

    SDL_Texture *tex = SDL_CreateTextureFromSurface(tc->renderer, surface);
    if (!tex) return NULL;
    tc_entry *e = insert(tc, full, tex);
    return e ? e->tex : tex;
}

SDL_Texture *texture_cache_draw_fit(texture_cache *tc, const char *path,
                                   SDL_Rect dst, SDL_Rect *out_rect)
{
    SDL_Texture *tex = texture_cache_load_file(tc, path);
    if (!tex) return NULL;
    int tw = 0, th = 0;
    SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
    if (tw <= 0 || th <= 0) return NULL;

    float scale = (float)dst.w / (float)tw;
    float sy = (float)dst.h / (float)th;
    if (sy < scale) scale = sy;

    int w = (int)(tw * scale + 0.5f);
    int h = (int)(th * scale + 0.5f);
    SDL_Rect r = { dst.x + (dst.w - w) / 2, dst.y + (dst.h - h) / 2, w, h };
    SDL_RenderCopy(tc->renderer, tex, NULL, &r);
    if (out_rect) *out_rect = r;
    return tex;
}
