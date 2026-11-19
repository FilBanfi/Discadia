/*
 * menu_screen.c - Start-button overlay menu.
 *
 * Drawn over the previous screen (the main loop renders exactly one
 * vtable per frame, so this screen redraws the home background itself).
 * Entries: Settings / Store / System info / Reboot / Shutdown / Quit.
 * Destructive actions (reboot/shutdown/quit) ask for confirmation via a
 * modal; shutdown/reboot honor settings.confirm_shutdown.
 */

#include "ui/menu_screen.h"
#include "ui/widgets.h"
#include "ui/renderer.h"
#include "ui/toast.h"
#include "ui/carousel.h"
#include "ui/empty_screen.h"
#include "ui/top_bar.h"
#include "library/library.h"
#include "i18n/locale.h"
#include "util/string_util.h"
#include "system/power.h"
#include "system/system_info.h"
#include "settings/settings_store.h"
#include "log.h"

typedef struct {
    int selected;          /* current entry index            */
    int confirm_idx;       /* entry awaiting Yes/No, or -1   */
    int yes_selected;      /* modal button focus             */
} menu_state;

static menu_state st = { 0, -1, 1 };

/* Menu entries in display order. */
enum { MI_SETTINGS = 0, MI_STORE, MI_INFO, MI_REBOOT, MI_SHUTDOWN, MI_QUIT,
       MI_COUNT };

static const char *menu_label(int i)
{
    switch (i)
    {
        case MI_SETTINGS: return locale_get("settings");
        case MI_STORE:    return locale_get("store");
        case MI_INFO:     return locale_get("system_info");
        case MI_REBOOT:   return locale_get("reboot");
        case MI_SHUTDOWN: return locale_get("shutdown");
        default:          return locale_get("quit");
    }
}

static const char *menu_confirm_key(int i)
{
    switch (i)
    {
        case MI_REBOOT:   return "confirm_reboot";
        case MI_SHUTDOWN: return "confirm_shutdown";
        default:          return "confirm_quit";
    }
}

static void menu_enter(app *a)
{
    st.selected = 0;
    st.confirm_idx = -1;
    st.yes_selected = 1;
    (void)a;
}

static void menu_leave(app *a) { (void)a; }

/* Perform the action of entry `i` (after any confirmation). */
static void menu_execute(app *a, int i)
{
    switch (i)
    {
        case MI_SETTINGS: app_request_nav(a, NAV_OPEN_SETTINGS); break;
        case MI_STORE:    app_request_nav(a, NAV_OPEN_STORE);    break;
        case MI_INFO:
        {
            /* Non-destructive: surface hardware info as toasts. */
            char msg[192];
            str_format(msg, sizeof(msg), "%s: %s (%ld MB)",
                       locale_get("system_info"), sysinfo_model(),
                       sysinfo_total_ram_mb());
            toast_push(app_toasts(a), msg);
            str_format(msg, sizeof(msg), "%s: %s",
                       locale_get("app_title"), app_version());
            toast_push(app_toasts(a), msg);
            break;
        }
        case MI_REBOOT:
            settings_store_save(app_settings(a));
            power_reboot();                    /* does not return on ok */
            toast_push_error(app_toasts(a), locale_get("error_launch_failed"));
            break;
        case MI_SHUTDOWN:
            settings_store_save(app_settings(a));
            power_shutdown();
            toast_push_error(app_toasts(a), locale_get("error_launch_failed"));
            break;
        case MI_QUIT:
            app_quit(a);
            break;
    }
}

static void menu_update(app *a, float dt_ms, const frame_input *in,
                        nav_request_t *out_nav)
{
    (void)dt_ms;

    if (st.confirm_idx >= 0)
    {
        /* Modal mode: left/right pick the button, confirm/back resolve. */
        if (in->left || in->right)
        {
            st.yes_selected = !st.yes_selected;
            app_play_sound(a, "navigate");
        }
        if (in->confirm)
        {
            int idx = st.confirm_idx;
            st.confirm_idx = -1;
            if (st.yes_selected) menu_execute(a, idx);
            else app_play_sound(a, "back");
        }
        else if (in->back)
        {
            st.confirm_idx = -1;               /* dismiss modal */
            app_play_sound(a, "back");
        }
        return;
    }

    if (in->up)     { st.selected = (st.selected + MI_COUNT - 1) % MI_COUNT;
                      app_play_sound(a, "navigate"); }
    if (in->down)   { st.selected = (st.selected + 1) % MI_COUNT;
                      app_play_sound(a, "navigate"); }

    if (in->confirm)
    {
        int i = st.selected;
        int destructive = (i == MI_REBOOT || i == MI_SHUTDOWN || i == MI_QUIT);
        int need_confirm = (i == MI_SHUTDOWN || i == MI_REBOOT)
                               ? app_settings(a)->confirm_shutdown
                               : (i == MI_QUIT);
        if (destructive && need_confirm)
        {
            st.confirm_idx = i;
            st.yes_selected = 0;               /* safe default: No */
            app_play_sound(a, "select");
        }
        else
        {
            app_play_sound(a, "select");
            menu_execute(a, i);
        }
    }
    else if (in->back || in->menu)
    {
        *out_nav = NAV_BACK;                   /* -> Home */
    }
}

static void menu_draw(app *a, float dt_ms)
{
    discadia_theme *t = app_theme(a);
    layout *L = app_layout(a);
    widget_ctx w = app_widget_ctx(a);

    /* Background: keep the home scene visible behind the overlay. */
    renderer_draw_background(app_renderer(a), t);
    int n = library_count(app_library(a));
    if (n > 0)
    {
        const library_item *sel = library_get(app_library(a),
                                              app_selected_item(a));
        carousel_draw(a, dt_ms);
        top_bar_draw(a, sel ? sel->title : NULL);
    }
    else
    {
        top_bar_draw(a, NULL);
        empty_screen_draw(a);
    }

    /* Dim layer under the panel. */
    renderer_fill_rounded(app_renderer(a), 0, 0, L->sw, L->sh, 0,
                          (SDL_Color){0, 0, 0, 140}, 140);

    /* Centered panel. */
    int pw = L->sw / 3, row_h = 56;
    if (pw < 320) pw = (L->sw < 360) ? L->sw - 24 : 320;
    int ph = row_h * MI_COUNT + 110;
    int px = (L->sw - pw) / 2, py = (L->sh - ph) / 2;

    renderer_fill_rounded(app_renderer(a), px, py, pw, ph, 14,
                          theme_color(t, TCOL_TOAST_BG), 250);
    renderer_outline_rounded(app_renderer(a), px, py, pw, ph, 14,
                             theme_color(t, TCOL_TOAST_BORDER), 2);

    widget_text_primary(&w, FACE_TITLE, t->sizes.title_size,
                        locale_get("menu"), px + pw / 2, py + 20, 0,
                        pw - 20, 1);

    for (int i = 0; i < MI_COUNT; i++)
        widget_list_row(&w, px + 14, py + 70 + i * row_h, pw - 28, row_h - 8,
                        menu_label(i), NULL, i == st.selected);

    if (st.confirm_idx >= 0)
        widget_confirm_box(&w, L->sw, L->sh,
                           locale_get(menu_confirm_key(st.confirm_idx)),
                           st.yes_selected);
}

const screen_vtable *screen_menu_vtable(void)
{
    static const screen_vtable vt = {
        .name = "menu", .enter = menu_enter, .update = menu_update,
        .draw = menu_draw, .leave = menu_leave
    };
    return &vt;
}
