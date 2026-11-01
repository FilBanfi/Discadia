/*
 * app_state.c - screen-local persistent state for the application.
 *
 * Screens keep their own static state (menu selection, store model,
 * scroll offsets...). This module owns the *cross-screen* pieces that
 * must survive navigation: the fade timer and the "back returns to"
 * bookkeeping. Kept tiny on purpose - everything else lives in app.c.
 */

#include "app/app.h"
#include <string.h>

static float s_fade_ms = 0.f;
static app_screen_t s_home_return_target = SCREEN_HOME;

void app_state_begin_fade(app *a)
{
    discadia_theme *t = app_theme(a);
    if (t && t->animations.enabled)
        s_fade_ms = (float)t->animations.fade_transition_ms;
    else
        s_fade_ms = 0.f;
}

void app_state_update_fade(float dt_ms)
{
    if (s_fade_ms > 0.f)
    {
        s_fade_ms -= dt_ms;
        if (s_fade_ms < 0.f) s_fade_ms = 0.f;
    }
}

float app_state_fade_alpha(void)
{
    /* 255 -> fully faded-to-black at transition start, 0 when done. */
    if (s_fade_ms <= 0.f) return 0.f;
    return s_fade_ms / 300.f;   /* normalized remaining fraction */
}

void app_state_set_return_target(app_screen_t s) { s_home_return_target = s; }
app_screen_t app_state_return_target(void)       { return s_home_return_target; }
