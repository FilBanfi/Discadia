/*
 * navigation.c - translates per-frame nav_request_t values into real
 * screen transitions, applying the documented flows:
 *
 *   Home -> Start -> Menu -> Settings -> Back -> Home (carousel kept)
 *   Home -> A on item -> Launching -> game exits -> Home
 *   Any settings sub-screen -> Back -> parent screen
 *
 * All state preservation (selected index, carousel position) happens
 * implicitly because those live in the app context, not in screens.
 */

#include "app/app.h"
#include "i18n/locale.h"
#include "ui/toast.h"
#include "engine/launcher.h"
#include "library/library.h"
#include "log.h"

#include <string.h>

/* Implemented in app_state.c */
void app_state_begin_fade(app *a);
void app_state_update_fade(float dt_ms);

/* Implemented in main loop helper (app.c). */
extern void app_quit(app *a);

static const char *launch_error_key(launch_result_t r)
{
    switch (r)
    {
        case LAUNCH_ERR_ENGINE_MISSING: return "error_engine_not_found";
        case LAUNCH_ERR_GAME_MISSING:   return "error_game_file_missing";
        default:                        return "error_launch_failed";
    }
}

void app_handle_navigation(app *a, nav_request_t nav)
{
    if (!a || nav == NAV_NONE) return;

    switch (nav)
    {
        case NAV_BACK:
        {
            app_screen_t cur = app_current_screen(a);
            switch (cur)
            {
                case SCREEN_THEME_SELECTOR:
                case SCREEN_LANGUAGE_SELECTOR:
                case SCREEN_INPUT_REMAP:
                    app_set_screen(a, SCREEN_SETTINGS);
                    app_play_sound(a, "back");
                    break;
                case SCREEN_SETTINGS:
                case SCREEN_STORE:
                case SCREEN_MENU:
                    app_set_screen(a, SCREEN_HOME);
                    app_play_sound(a, "back");
                    break;
                case SCREEN_LAUNCHING:
                    /* Only allowed after the child exited; screens guard
                     * this themselves so we just ignore stray BACKs. */
                    break;
                default:
                    break;   /* HOME: back does nothing (no accidental quit) */
            }
            break;
        }
        case NAV_OPEN_MENU:
            if (app_current_screen(a) == SCREEN_MENU)
                app_set_screen(a, SCREEN_HOME);
            else
                app_set_screen(a, SCREEN_MENU);
            app_play_sound(a, "select");
            break;
        case NAV_OPEN_SETTINGS: app_set_screen(a, SCREEN_SETTINGS); break;
        case NAV_OPEN_STORE:    app_set_screen(a, SCREEN_STORE);    break;
        case NAV_LAUNCH_ITEM:
        {
            const library_item *it =
                library_get(app_library(a), app_selected_item(a));
            if (!it) { app_set_screen(a, SCREEN_HOME); break; }
            app_set_launch_title(a, it->title);
            launcher *l = app_launcher(a);
            launch_result_t r = launcher_start(l, it, app_engines(a),
                                               app_saves_base(a));
            if (r == LAUNCH_OK)
            {
                app_set_launch_error(a, 0);
                app_set_screen(a, SCREEN_LAUNCHING);
                app_play_sound(a, "select");
            }
            else
            {
                char msg[512];
                const char *ph = (r == LAUNCH_ERR_ENGINE_MISSING)
                                     ? l->error_detail      /* engine id */
                                     : it->game_path;       /* file path */
                locale_fmt(msg, sizeof(msg), launch_error_key(r),
                           r == LAUNCH_ERR_ENGINE_MISSING ? "engine" : "path",
                           ph, NULL, NULL);
                toast_push_error(app_toasts(a), msg);
                app_play_sound(a, "back");
            }
            break;
        }
        case NAV_QUIT:     app_quit(a); break;
        case NAV_SHUTDOWN:
            app_shutdown(a);
            extern int power_shutdown(void);
            power_shutdown();
            app_quit(a);      /* only reached if power command failed */
            break;
        case NAV_REBOOT:
            app_shutdown(a);
            extern int power_reboot(void);
            power_reboot();
            app_quit(a);
            break;
        default: break;
    }
}
