/*
 * home_screen.c - the main library screen: top bar, carousel, metadata
 * panel and empty state. Navigation: L/R move, A launches, Start menu,
 * Back exits when in a sub-flow (handled by navigation layer).
 */

#include "ui/home_screen.h"
#include "ui/carousel.h"
#include "ui/top_bar.h"
#include "ui/metadata_panel.h"
#include "ui/empty_screen.h"
#include "ui/renderer.h"
#include "library/library.h"
#include "log.h"

static void home_enter(app *a)      { (void)a; }
static void home_leave(app *a)      { (void)a; }

static void home_update(app *a, float dt_ms, const frame_input *in,
                        nav_request_t *out_nav)
{
    int nav = 0;
    carousel_update(a, dt_ms, in, &nav);
    if (in->confirm && library_count(app_library(a)) > 0)
        *out_nav = NAV_LAUNCH_ITEM;
    else if (in->menu)
        *out_nav = NAV_OPEN_MENU;
}

static void home_draw(app *a, float dt_ms)
{
    renderer_draw_background(app_renderer(a), app_theme(a));
    layout *L = app_layout(a);
    (void)L;
    int n = library_count(app_library(a));
    if (n <= 0)
    {
        top_bar_draw(a, NULL);
        empty_screen_draw(a);
        return;
    }
    carousel_draw(a, dt_ms);
    const library_item *sel = library_get(app_library(a),
                                          app_selected_item(a));
    top_bar_draw(a, sel ? sel->title : NULL);
    metadata_panel_draw(a, sel);
}

const screen_vtable *screen_home_vtable(void)
{
    static const screen_vtable vt = {
        .name = "home", .enter = home_enter, .update = home_update,
        .draw = home_draw, .leave = home_leave
    };
    return &vt;
}
