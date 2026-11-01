/*
 * font_cache.c - TTF face registry + LRU surface cache.
 * Keys are (face name, size, color RGBA, wrap width, max lines, text).
 * The cache is a singly-linked list kept in MRU order; when it exceeds
 * FC_MAX_SURFACES entries the tail is freed. Surfaces are white-on-alpha
 * and modulated at draw time so one entry serves every color: we render
 * with SDL_RenderTextBlended white and store the requested color only
 * for key matching simplicity.
 */

#include "ui/font_cache.h"
#include "util/string_util.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define FC_MAX_FACES     8
#define FC_MAX_SURFACES  256
#define FC_NAME_LEN      32

typedef struct {
    char     name[FC_NAME_LEN];
    TTF_Font *font;
} fc_face;

typedef struct fc_entry {
    struct fc_entry *next;
    char  face[FC_NAME_LEN];
    int   size;
    Uint32 color_key;          /* packed RGBA */
    int   max_width, max_lines;
    char *text;
    SDL_Surface *surface;
} fc_entry;

struct font_cache {
    fc_face  faces[FC_MAX_FACES];
    int      face_count;
    fc_entry *head;            /* MRU list */
    int      count;
};

font_cache *font_cache_create(void)
{
    if (!SDL_WasInit(SDL_INIT_VIDEO))
        SDL_InitSubSystem(SDL_INIT_VIDEO);
    if (!TTF_WasInit() && TTF_Init() != 0)
        LOG_ERROR("TTF_Init failed: %s", TTF_GetError());
    return calloc(1, sizeof(struct font_cache));
}

static void free_entry(fc_entry *e)
{
    if (!e) return;
    if (e->surface) SDL_FreeSurface(e->surface);
    free(e->text);
    free(e);
}

void font_cache_destroy(font_cache *fc)
{
    if (!fc) return;
    fc_entry *e = fc->head;
    while (e) { fc_entry *n = e->next; free_entry(e); e = n; }
    for (int i = 0; i < fc->face_count; i++)
        if (fc->faces[i].font) TTF_CloseFont(fc->faces[i].font);
    free(fc);
}

int font_cache_add_face(font_cache *fc, const char *name,
                        const char *path, int size, const char *fallback_path)
{
    if (!fc || !name || size <= 0) return -1;
    if (fc->face_count >= FC_MAX_FACES)
    {
        LOG_WARN("font cache face table full, ignoring '%s'", name);
        return -1;
    }

    TTF_Font *f = NULL;
    if (path && *path) f = TTF_OpenFont(path, size);
    if (!f && fallback_path && *fallback_path)
        f = TTF_OpenFont(fallback_path, size);
    if (!f)
    {
        LOG_WARN("cannot open font '%s' (size %d): %s",
                 path ? path : "(none)", size, TTF_GetError());
        return -1;
    }

    fc_face *slot = &fc->faces[fc->face_count++];
    str_safe_copy(slot->name, name, sizeof(slot->name));
    slot->font = f;
    return 0;
}

static fc_face *find_face(font_cache *fc, const char *name)
{
    for (int i = 0; i < fc->face_count; i++)
        if (strcmp(fc->faces[i].name, name) == 0) return &fc->faces[i];
    return NULL;
}

void font_cache_flush(font_cache *fc)
{
    if (!fc) return;
    fc_entry *e = fc->head;
    fc->head = NULL;
    fc->count = 0;
    while (e) { fc_entry *n = e->next; free_entry(e); e = n; }
}

static Uint32 pack_color(SDL_Color c)
{
    return ((Uint32)c.r << 24) | ((Uint32)c.g << 16) |
           ((Uint32)c.b << 8) | (Uint32)c.a;
}

static fc_entry *cache_find_move_mru(font_cache *fc, const char *face, int size,
                                     Uint32 ck, int mw, int ml, const char *text)
{
    fc_entry *prev = NULL, *cur = fc->head;
    while (cur)
    {
        if (cur->size == size && cur->color_key == ck &&
            cur->max_width == mw && cur->max_lines == ml &&
            strcmp(cur->face, face) == 0 && strcmp(cur->text, text) == 0)
        {
            if (prev) { prev->next = cur->next; cur->next = fc->head; fc->head = cur; }
            return cur;
        }
        prev = cur; cur = cur->next;
    }
    return NULL;
}

static void cache_evict_if_needed(font_cache *fc)
{
    while (fc->count > FC_MAX_SURFACES && fc->head)
    {
        fc_entry *prev = NULL, *cur = fc->head;
        while (cur->next) { prev = cur; cur = cur->next; }
        if (prev) prev->next = NULL; else fc->head = NULL;
        free_entry(cur);
        fc->count--;
    }
}

/* Render text possibly wrapped to max_width, limited to max_lines with
 * ellipsis on the last allowed line. Returns owned surface. */
static SDL_Surface *render_text(TTF_Font *font, const char *utf8,
                                SDL_Color color, int max_width, int max_lines)
{
    if (max_width <= 0 && max_lines <= 0)
        return TTF_RenderUTF8_Blended(font, utf8, color);

    /* Split into words, greedily fill lines. */
    char work[4096];
    str_safe_copy(work, utf8, sizeof(work));

    const char *lines[64];
    int nl = 0;
    char *save = NULL;
    char *saved_lines[64];
    int saved_n = 0;

    /* Break explicit newlines first, then wrap each paragraph. */
    for (char *para = strtok_r(work, "\n", &save); para && nl < 64;
         para = strtok_r(NULL, "\n", &save))
    {
        char *inner_save = NULL;
        char current[1024] = "";
        for (char *word = strtok_r(para, " ", &inner_save); word;
             word = strtok_r(NULL, " ", &inner_save))
        {
            char trial[1200];
            if (*current) snprintf(trial, sizeof(trial), "%s %s", current, word);
            else          snprintf(trial, sizeof(trial), "%s", word);

            int tw = 0;
            TTF_SizeUTF8(font, trial, &tw, NULL);
            if (tw <= max_width || !*current)
            {
                str_safe_copy(current, trial, sizeof(current));
            }
            else
            {
                saved_lines[saved_n++] = strdup(current); /* leak-checked below */
                if (saved_n >= 64) break;
                snprintf(current, sizeof(current), "%s", word);
            }
        }
        if (saved_n < 64 && *current)
            saved_lines[saved_n++] = strdup(current);
    }

    int total = saved_n;
    if (max_lines > 0 && total > max_lines) total = max_lines;
    for (int i = 0; i < total; i++) lines[i] = saved_lines[i];

    /* Build surface: measure, allocate, blit line by line. */
    int maxw = 0, hsum = 0;
    for (int i = 0; i < total; i++)
    {
        int lw = 0, lh = 0;
        TTF_SizeUTF8(font, lines[i], &lw, &lh);
        if (lw > maxw) maxw = lw;
        hsum += lh;
    }

    SDL_Surface *out = NULL;
    if (total > 0)
    {
        out = SDL_CreateRGBSurfaceWithFormat(0, maxw + 4, hsum + 4, 32,
                                             SDL_PIXELFORMAT_RGBA32);
        if (out)
        {
            int y = 2;
            for (int i = 0; i < total; i++)
            {
                const char *txt = lines[i];
                char ell[1024];
                ell[0] = '\0';
                if (max_lines > 0 && i == total - 1 && saved_n > total)
                {
                    /* Truncate to fit "…" suffix: cut characters from the
                     * end of this line until "<cut>..." fits max_width. */
                    str_safe_copy(ell, txt, sizeof(ell));
                    while (*ell)
                    {
                        char trial[1100];
                        snprintf(trial, sizeof(trial), "%s...", ell);
                        int tw = 0;
                        TTF_SizeUTF8(font, trial, &tw, NULL);
                        if (tw <= max_width) { strcpy(ell, trial); break; }
                        size_t len = strlen(ell);
                        if (len <= 1) { ell[0] = '\0'; break; }
                        ell[len - 1] = '\0';
                    }
                    txt = ell;
                }
                SDL_Surface *ls = TTF_RenderUTF8_Blended(font, txt, color);
                if (ls)
                {
                    SDL_Rect dst = { 2, y, 0, 0 };
                    SDL_BlitSurface(ls, NULL, out, &dst);
                    y += ls->h;
                    SDL_FreeSurface(ls);
                }
                else
                {
                    int lh = TTF_FontLineSkip(font);
                    y += lh;
                }
            }
        }
    }

    for (int i = 0; i < saved_n; i++) free(saved_lines[i]);
    return out;
}

SDL_Surface *font_cache_text(font_cache *fc, const char *face, int size,
                             SDL_Color color, const char *utf8,
                             int max_width, int max_lines)
{
    if (!fc || !face || !utf8 || size <= 0) return NULL;

    Uint32 ck = pack_color(color);
    fc_entry *hit = cache_find_move_mru(fc, face, size, ck,
                                        max_width, max_lines, utf8);
    if (hit) return hit->surface;

    fc_face *f = find_face(fc, face);
    if (!f || !f->font) return NULL;

    SDL_Surface *surf = render_text(f->font, utf8, color, max_width, max_lines);
    if (!surf) return NULL;

    fc_entry *e = calloc(1, sizeof(*e));
    if (!e) { SDL_FreeSurface(surf); return NULL; }
    e->surface = surf;
    e->text = strdup(utf8);
    str_safe_copy(e->face, face, sizeof(e->face));
    e->size = size;
    e->color_key = ck;
    e->max_width = max_width;
    e->max_lines = max_lines;

    e->next = fc->head;
    fc->head = e;
    fc->count++;
    cache_evict_if_needed(fc);
    return surf;
}

int font_cache_measure(font_cache *fc, const char *face, int size,
                       const char *utf8)
{
    (void)size;
    if (!fc || !face || !utf8) return 0;
    fc_face *f = find_face(fc, face);
    if (!f || !f->font) return 0;
    int w = 0;
    TTF_SizeUTF8(f->font, utf8, &w, NULL);
    return w;
}
