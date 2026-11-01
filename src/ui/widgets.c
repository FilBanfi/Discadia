/*
 * widgets.c - shared themed drawing helpers.
 */

#include "ui/widgets.h"
#include "i18n/locale.h"
#include "util/string_util.h"
#include "log.h"

#include <string.h>
#include <ctype.h>

int widget_text(widget_ctx *w, const char *face, int size, SDL_Color color,
                const char *utf8, int x, int y, int align,
                int max_width, int max_lines)
{
    if (!w || !w->ren || !w->fcache || !utf8) return 0;
    SDL_Surface *surf = font_cache_text(w->fcache, face, size, color, utf8,
                                        max_width, max_lines);
    if (!surf) return 0;

    /* Texture key includes surface pointer: identical text+color reuses
     * both the surface and its uploaded texture. */
    char key[256];
    str_format(key, sizeof(key), "%s/%d/%dx%d@%p", face, size,
               surf->w, surf->h, (void *)surf);
    SDL_Texture *tex = texture_cache_from_surface(w->tcache, key, surf);
    if (!tex) return 0;

    SDL_Rect dst;
    dst.w = surf->w;
    dst.h = surf->h;
    if (align == 0)      dst.x = x - surf->w / 2;
    else if (align == 1) dst.x = x - surf->w;
    else                 dst.x = x;
    dst.y = y;

    SDL_SetTextureColorMod(tex, 255, 255, 255);
    SDL_RenderCopy(renderer_sdl(w->ren), tex, NULL, &dst);
    return surf->h;
}

int widget_text_primary(widget_ctx *w, const char *face, int size,
                        const char *utf8, int x, int y, int align,
                        int max_width, int max_lines)
{
    return widget_text(w, face, size, theme_color(w->theme, TCOL_TEXT),
                       utf8, x, y, align, max_width, max_lines);
}

int widget_text_secondary(widget_ctx *w, const char *face, int size,
                          const char *utf8, int x, int y, int align,
                          int max_width, int max_lines)
{
    return widget_text(w, face, size, theme_color(w->theme, TCOL_TEXT_SECONDARY),
                       utf8, x, y, align, max_width, max_lines);
}

void widget_placeholder_cover(widget_ctx *w, int x, int y, int size,
                              const char *title)
{
    if (!w || size <= 0) return;
    SDL_Color bg = theme_color(w->theme, TCOL_SECONDARY);
    renderer_fill_rounded(w->ren, x, y, size, size,
                          w->theme->sizes.icon_border_radius, bg, 255);

    /* First two letters of the title as a monogram. */
    char mono[8] = "?";
    if (title && *title)
    {
        /* Skip articles ("the ", "a ", "l'", ...) for nicer initials. */
        const char *t = title;
        mono[0] = (char)toupper((unsigned char)*t);
        const char *sp = strchr(t, ' ');
        if (sp && sp[1]) mono[1] = (char)toupper((unsigned char)sp[1]);
        else mono[1] = '\0';
        mono[2] = '\0';
    }
    int fs = size / 3;
    widget_text(w, FACE_TITLE, fs, theme_color(w->theme, TCOL_TEXT),
                mono, x + size / 2, y + size / 2 - fs / 2, 0, 0, 0);
}

void widget_cover(widget_ctx *w, int cx, int cy, int size,
                  const char *image_path, const char *fallback_title,
                  int selected)
{
    if (!w) return;
    int x = cx - size / 2, y = cy - size / 2;

    if (w->theme->effects.icon_shadow)
        renderer_fill_rounded(w->ren, x + 4, y + 6, size, size,
                              w->theme->sizes.icon_border_radius,
                              (SDL_Color){0, 0, 0, 120}, 120);

    SDL_Texture *tex = NULL;
    if (image_path && *image_path)
    {
        SDL_Rect dst = { x, y, size, size };
        tex = texture_cache_draw_fit(w->tcache, image_path, dst, NULL);
    }
    if (!tex)
        widget_placeholder_cover(w, x, y, size, fallback_title);

    int rad = w->theme->sizes.icon_border_radius;
    if (selected)
    {
        if (w->theme->effects.selection_glow)
            renderer_glow_rect(w->ren, x, y, size, size, rad,
                               theme_color(w->theme, TCOL_SELECTION_GLOW), 6);
        renderer_outline_rounded(w->ren, x, y, size, size, rad,
                                 theme_color(w->theme, TCOL_ACCENT), 3);
    }
    else
    {
        renderer_outline_rounded(w->ren, x, y, size, size, rad,
                                 theme_color(w->theme, TCOL_SECONDARY), 1);
    }
}

void widget_list_row(widget_ctx *w, int x, int y, int wpx, int h,
                     const char *label, const char *value, int selected)
{
    if (!w) return;
    if (selected)
    {
        renderer_fill_rounded(w->ren, x, y, wpx, h, h / 4,
                              theme_color(w->theme, TCOL_PRIMARY), 60);
        renderer_hline(w->ren, x, y, wpx,
                       theme_color(w->theme, TCOL_ACCENT), 200);
        renderer_hline(w->ren, x, y + h - 1, wpx,
                       theme_color(w->theme, TCOL_ACCENT), 200);
    }
    int pad = h / 4;
    int tsz = h * 6 / 13;
    widget_text(w, FACE_BODY, tsz,
                theme_color(w->theme, selected ? TCOL_TEXT : TCOL_TEXT_SECONDARY),
                label ? label : "", x + pad, y + (h - tsz) / 2 - 2, -1, 0, 0);
    if (value && *value)
        widget_text(w, FACE_BODY, tsz,
                    theme_color(w->theme, TCOL_ACCENT),
                    value, x + wpx - pad, y + (h - tsz) / 2 - 2, 1, 0, 0);
}

void widget_confirm_box(widget_ctx *w, int sw, int sh, const char *question,
                        int yes_selected)
{
    if (!w) return;
    int bw = sw / 3, bh = sh / 4;
    if (bw < 360) bw = (sw < 400) ? sw - 20 : 360;
    int bx = (sw - bw) / 2, by = (sh - bh) / 2;

    /* Dim the whole screen behind the modal. */
    renderer_fill_rounded(w->ren, 0, 0, sw, sh, 0, (SDL_Color){0, 0, 0, 160}, 160);
    renderer_fill_rounded(w->ren, bx, by, bw, bh, 12,
                          theme_color(w->theme, TCOL_TOAST_BG), 245);
    renderer_outline_rounded(w->ren, bx, by, bw, bh, 12,
                             theme_color(w->theme, TCOL_TOAST_BORDER), 2);

    widget_text_primary(w, FACE_BODY, bh / 7, question,
                        bx + bw / 2, by + bh / 6, 0, bw - 30, 2);

    /* Two buttons side by side at the bottom of the box. */
    int btn_w = bw / 3, btn_h = bh / 5;
    int btn_y = by + bh - btn_h - bh / 8;
    const char *yes_s = locale_get("yes"), *no_s = locale_get("no");
    for (int i = 0; i < 2; i++)
    {
        int sel = (i == 0) == (yes_selected != 0);
        int bxx = bx + bw / 6 + i * (btn_w + bw / 6);
        SDL_Color base = sel ? theme_color(w->theme, TCOL_PRIMARY)
                             : theme_color(w->theme, TCOL_SECONDARY);
        renderer_fill_rounded(w->ren, bxx, btn_y, btn_w, btn_h, btn_h / 4,
                              base, sel ? 255 : 180);
        widget_text(w, FACE_BODY, btn_h * 2 / 5,
                    theme_color(w->theme, TCOL_TEXT),
                    (i == 0) ? yes_s : no_s,
                    bxx + btn_w / 2, btn_y + btn_h / 8, 0, 0, 0);
    }
}
