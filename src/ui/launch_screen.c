/*
 * launch_screen.c - "Launching..." overlay shown while a game process
 * runs. Keeps animating (non-blocking polling of the child), and when
 * the game exits it returns to Home with the carousel untouched.
 */

#include "ui/launch_screen.h"
#include "ui/widgets.h"
#include "ui/renderer.h"
#include "ui/animation.h"
#include "engine/launcher.h"
#include "i18n/locale.h"
#include "input/input.h"

static void launch_enter(app *a)
{
    app_set_launch_error(a, 0);
}

static void launch_update(app *a, float dt_ms, const frame_input *in,
                          nav_request_t *out_nav)
{
    (void)dt_ms; (void)in;
    launcher *l = app_launcher(a);

    /* Drain SDL events so hotplug/window-close still work while the
     * game owns the screen. */
    input_pump_pending_events();

    if (!launcher_update(l))
    {
        /* Child finished (or was never really running): go home. */
        *out_nav = NAV_BACK;   /* navigation maps LAUNCHING->BACK to HOME */
        app_set_screen(a, SCREEN_HOME);
    }
}

static void launch_draw(app *a, float dt_ms)
{
    discadia_theme *t = app_theme(a);
    layout *L = app_layout(a);
    widget_ctx w = app_widget_ctx(a);

    renderer_draw_background(app_renderer(a), t);

    static float age = 0.f;
    age += dt_ms;

    int cx = L->sw / 2, cy = L->sh * 45 / 100;

    /* Title of the game being launched. */
    widget_text_primary(&w, FACE_TITLE, t->sizes.title_size,
                        app_launch_title(a), cx,
                        cy - t->sizes.title_size, 0, L->sw - 40, 1);

    /* Localized "Launching..." message with pulsing alpha. */
    int pulse = (int)(150 + 105 * anim_ease_out_quad(
                                   anim_pingpong_ms(age, 1200.f)));
    SDL_Color txt = theme_color(t, TCOL_TEXT_SECONDARY);
    txt.a = (Uint8)pulse;
    widget_text(&w, FACE_BODY, t->sizes.description_size, txt,
                locale_get("launching"), cx, cy + 10, 0, L->sw - 40, 1);

    /* Spinning progress ring under the text. */
    SDL_Renderer *r = renderer_sdl(app_renderer(a));
    SDL_Color acc = theme_color(t, TCOL_ACCENT);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    int rad = t->sizes.description_size;
    int ry = cy + t->sizes.description_size + 40;
    for (int i = 0; i < 32; i++)
    {
        double ang = (i / 32.0) * 2 * M_PI + age / 350.0;
        Uint8 al = (Uint8)(30 + 200 * (i / 32.0));
        SDL_SetRenderDrawColor(r, acc.r, acc.g, acc.b, al);
        SDL_RenderDrawPoint(r, cx + (int)(cos(ang) * rad),
                               ry + (int)(sin(ang) * rad));
    }
}

static void launch_leave(app *a) { (void)a; }

const screen_vtable *screen_launching_vtable(void)
{
    static const screen_vtable vt = {
        .name = "launching", .enter = launch_enter, .update = launch_update,
        .draw = launch_draw, .leave = launch_leave
    };
    return &vt;
}
