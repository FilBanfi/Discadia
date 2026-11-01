#ifndef DISCADIA_APP_H
#define DISCADIA_APP_H

/*
 * Application context: the single object threaded through every module.
 * Holds runtime settings, resolved data paths, theme, locale, library,
 * media state and UI subsystems. Created in main(), destroyed on exit.
 * Screens receive only this pointer - no global state anywhere else.
 */

#include <SDL.h>
#include "settings/settings_model.h"
#include "theme/theme.h"

typedef struct renderer      renderer;
typedef struct texture_cache texture_cache;
typedef struct font_cache    font_cache;
typedef struct audio         audio;
typedef struct library       library;
typedef struct disc_manager  disc_manager;
typedef struct engine_registry engine_registry;
typedef struct toast_queue   toast_queue;
typedef struct app_state     app_state;

typedef enum app_screen {
    SCREEN_BOOT = 0,
    SCREEN_HOME,
    SCREEN_MENU,
    SCREEN_SETTINGS,
    SCREEN_THEME_SELECTOR,
    SCREEN_LANGUAGE_SELECTOR,
    SCREEN_INPUT_REMAP,
    SCREEN_STORE,
    SCREEN_LAUNCHING,
    SCREEN_COUNT
} app_screen_t;

struct app;
typedef struct app app;

/* Actions requested by a screen for the current frame (navigation layer
 * translates them into state transitions). */
typedef enum nav_request {
    NAV_NONE = 0,
    NAV_BACK,          /* user pressed BACK inside a screen             */
    NAV_OPEN_SETTINGS,
    NAV_OPEN_STORE,
    NAV_OPEN_MENU,
    NAV_LAUNCH_ITEM,   /* item_index holds the library index to launch  */
    NAV_QUIT,
    NAV_SHUTDOWN,
    NAV_REBOOT
} nav_request_t;

/* Per-frame input snapshot handed to screens. */
typedef struct {
    int left, right, up, down, confirm, back, menu, context; /* edges    */
    int any_confirm_or_back;                                 /* modal    */
} frame_input;

/* Screen vtable. All callbacks must be non-NULL except draw_overlay. */
typedef struct screen_vtable {
    const char *name;
    void (*enter)(app *a);
    void (*update)(app *a, float dt_ms, const frame_input *in, nav_request_t *out_nav);
    void (*draw)(app *a, float dt_ms);
    void (*leave)(app *a);
} screen_vtable;

/* ---- app lifecycle (implemented in src/app/app.c) ---- */
app *app_create(void);
int  app_init(app *a);        /* SDL, config, i18n, theme, engines, lib */
void app_run(app *a);         /* boot -> main loop -> shutdown          */
void app_shutdown(app *a);    /* save config + free everything          */
void app_destroy(app *a);

/* ---- services used by screens ---- */
void app_set_screen(app *a, app_screen_t s);
app_screen_t app_current_screen(const app *a);
const char *app_data_dir(const app *a);
const char *app_cache_dir(const app *a);
engine_registry *app_engines(const app *a);
toast_queue *app_toasts(const app *a);
audio *app_audio_sys(const app *a);
disc_manager *app_discs(const app *a);
library *app_library(const app *a);
renderer *app_renderer(const app *a);
texture_cache *app_texcache(const app *a);
font_cache *app_fontcache(const app *a);
settings_model *app_settings(app *a);
discadia_theme *app_theme(app *a);
float app_carousel_pos(const app *a);      /* animated carousel position */
int   app_selected_item(const app *a);     /* logical selected index     */
int   app_launch_error(const app *a);      /* pending launch failed?     */
nav_request_t app_take_pending_nav(app *a);/* menu-driven transitions    */
void  app_play_sound(app *a, const char *logical_name);
void  app_rescan_library(app *a);
void  app_reapply_theme(app *a);           /* after theme change         */

/* Screen vtables (one per UI file). */
const screen_vtable *screen_boot_vtable(void);
const screen_vtable *screen_home_vtable(void);
const screen_vtable *screen_menu_vtable(void);
const screen_vtable *screen_settings_vtable(void);
const screen_vtable *screen_theme_selector_vtable(void);
const screen_vtable *screen_language_selector_vtable(void);
const screen_vtable *screen_input_remap_vtable(void);
const screen_vtable *screen_store_vtable(void);
const screen_vtable *screen_launching_vtable(void);

#endif /* DISCADIA_APP_H */
