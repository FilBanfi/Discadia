/*
 * boot_screen.c - procedural splash that replaces the host system boot
 * graphics (DietPi logo, kernel text, etc.). No external assets needed:
 * gradient background, glowing wordmark, spinning disc arc and version.
 * Auto-advances to Home after a short delay; any button skips it.
 */

#include "ui/boot_screen.h"
#include "ui/widgets.h"
#include "ui/renderer.h"
#include "ui/animation.h"
#include "i18n/locale.h"
#include "util/string_util.h"

#include <math.h>
#include <string.h>

#define BOOT_MIN_MS 900.f      /* minimum visible time                */
#define BOOT_AUTO_MS 2400.f    /* auto-advance when nobody presses    */

static void boot_enter(app *a)
{
    app_play_sound(a, "boot");
}

static void boot_update(app *a, float dt_ms, const frame_input *in,
                        nav_request_t *out_nav)
{
    static float age = 0.f;
    age += dt_ms;
    if (age >= BOOT_AUTO_MS || in->any_confirm_or_back)
    {
        age = 0.f;
        *out_nav = NAV_NONE;
        app_set_screen(a, SCREEN_HOME);
    }
}

static void boot_draw(app *a, float dt_ms)
{
    discadia_theme *t = app_theme(a);
    layout *L = app_layout(a);
    widget_ctx w = app_widget_ctx(a);
    SDL_Renderer *r = renderer_sdl(app_renderer(a));

    renderer_draw_background(app_renderer(a), t);

    static float age = 0.f;
    age += dt_ms;
    float p = anim_clamp(age / 700.f, 0.f, 1.f);   /* fade-in progress */

    int cx = L->sw / 2, cy = L->sh * 42 / 100;

    /* Spinning arc around the wordmark. */
    SDL_Color acc = theme_color(t, TCOL_ACCENT);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    int rad = L->icon_selected / 2 + 30;
    float rot = age / 900.f;
    for (int i = 0; i < 60; i++)
    {
        double ang = (i / 60.0) * 2 * M_PI + rot;
        Uint8 alpha = (Uint8)(40 + 150 * (i / 60.0));
        SDL_SetRenderDrawColor(r, acc.r, acc.g, acc.b, alpha);
        SDL_RenderDrawPoint(r, cx + (int)(cos(ang) * rad),
                               cy + (int)(sin(ang) * rad * 0.45f));
    }

    /* Glowing wordmark. */
    SDL_Color txt = theme_color(t, TCOL_TEXT);
    txt.a = (Uint8)(255 * p);
    widget_text(&w, FACE_TITLE, t->sizes.boot_title_size, txt,
                locale_get("app_title"), cx, cy - t->sizes.boot_title_size / 2,
                0, 0, 1);

    /* Version line under the title. */
    char ver[64];
    str_format(ver, sizeof(ver), "v%s", app_version());
    SDL_Color sec = theme_color(t, TCOL_TEXT_SECONDARY);
    sec.a = (Uint8)(200 * p);
    widget_text(&w, FACE_SMALL, t->sizes.meta_size, sec, ver, cx,
                cy + t->sizes.boot_title_size / 2 + 8, 0, 0, 1);

    /* Loading hint with animated dots. */
    if (age > BOOT_MIN_MS)
    {
        int dots = ((int)(age / 300.f)) % 4;
        char hint[64];
        str_format(hint, sizeof(hint), "%s%.*s", locale_get("boot_loading"),
                   dots, "...");
        widget_text_secondary(&w, FACE_BODY, t->sizes.description_size, hint,
                              cx, (int)(L->sh * 0.72f), 0, 0, 1);
    }
}

static void boot_leave(app *a) { (void)a; }

const screen_vtable *screen_boot_vtable(void)
{
    static const screen_vtable vt = {
        .name = "boot", .enter = boot_enter, .update = boot_update,
        .draw = boot_draw, .leave = boot_leave
    };
    return &vt;
}
