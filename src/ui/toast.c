/*
 * toast.c - see toast.h. Fixed-capacity ring buffer, zero allocations
 * after create().
 */

#include "ui/toast.h"
#include "util/string_util.h"
#include <string.h>

#define TOAST_MAX 8
#define TOAST_TEXT_LEN 256

typedef struct toast {
    char text[TOAST_TEXT_LEN];
    int  is_error;
    long ttl_ms;      /* remaining lifetime        */
    long age_ms;      /* total lifetime so far     */
} toast;

struct toast_queue {
    toast items[TOAST_MAX];
    int   head, count;
};

static long toast_default_ttl = 3500; /* may be overridden via setter below */

void toast_set_default_ttl_ms(long ms) { if (ms > 0) toast_default_ttl = ms; }

toast_queue *toast_queue_create(void)
{
    toast_queue *q = calloc(1, sizeof(*q));
    return q;
}

void toast_queue_destroy(toast_queue *q) { free(q); }

static void toast_push_raw(toast_queue *q, const char *text, int is_error)
{
    if (!q || !text) return;
    int slot = (q->head + q->count) % TOAST_MAX;
    if (q->count == TOAST_MAX)  /* overwrite oldest */
    {
        slot = q->head;
        q->head = (q->head + 1) % TOAST_MAX;
    }
    else q->count++;
    str_safe_copy(q->items[slot].text, text, TOAST_TEXT_LEN);
    q->items[slot].is_error = is_error;
    q->items[slot].ttl_ms = toast_default_ttl;
    q->items[slot].age_ms = 0;
}

void toast_push(toast_queue *q, const char *text) { toast_push_raw(q, text, 0); }
void toast_push_error(toast_queue *q, const char *text) { toast_push_raw(q, text, 1); }

void toast_update(toast_queue *q, float dt_ms)
{
    if (!q) return;
    int dt = (int)(dt_ms + 0.5f);
    for (int i = 0; i < q->count; )
    {
        int idx = (q->head + i) % TOAST_MAX;
        q->items[idx].age_ms += dt;
        q->items[idx].ttl_ms -= dt;
        if (q->items[idx].ttl_ms <= 0)
        {
            /* remove by shifting the rest down one slot */
            for (int j = i; j < q->count - 1; j++)
            {
                int from = (q->head + j + 1) % TOAST_MAX;
                int to   = (q->head + j) % TOAST_MAX;
                q->items[to] = q->items[from];
            }
            q->count--;
        }
        else i++;
    }
}

int toast_pending(const toast_queue *q) { return q ? q->count : 0; }

void toast_draw(toast_queue *q, widget_ctx *w, int sw)
{
    if (!q || !w || q->count == 0) return;

    int y = 8;
    int max_w = sw - 40;
    for (int i = 0; i < q->count; i++)
    {
        int idx = (q->head + i) % TOAST_MAX;
        toast *t = &q->items[idx];

        /* Fade out during the last 400ms. */
        int alpha = 230;
        if (t->ttl_ms < 400) alpha = (int)(230.0 * t->ttl_ms / 400.0);
        if (alpha < 0) alpha = 0;

        SDL_Surface *surf = font_cache_text(w->fcache, FACE_SMALL,
                                            w->theme->sizes.meta_size + 2,
                                            theme_color(w->theme, TCOL_TEXT),
                                            t->text, max_w, 1);
        if (!surf) continue;
        int bw = surf->w + 36, bh = surf->h + 16;
        int bx = (sw - bw) / 2;

        SDL_Color bg = theme_color(w->theme, TCOL_TOAST_BG);
        bg.a = (Uint8)alpha;
        renderer_fill_rounded(w->ren, bx, y, bw, bh, 10, bg, alpha);

        SDL_Color bc = theme_color(w->theme, t->is_error ? TCOL_PRIMARY
                                                          : TCOL_TOAST_BORDER);
        renderer_outline_rounded(w->ren, bx, y, bw, bh, 10, bc, 2);

        char key[192];
        str_format(key, sizeof(key), "toast/%p/%dx%d", (void *)surf,
                   surf->w, surf->h);
        SDL_Texture *tex = texture_cache_from_surface(w->tcache, key, surf);
        if (tex)
        {
            SDL_Rect dr = { bx + 18, y + 8, surf->w, surf->h };
            SDL_SetTextureAlphaMod(tex, (Uint8)alpha);
            SDL_RenderCopy(renderer_sdl(w->ren), tex, NULL, &dr);
        }
        y += bh + 8;
    }
}
