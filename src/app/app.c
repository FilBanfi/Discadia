/*
 * app.c - application context, lifecycle and the 60fps main loop.
 *
 * Owns every subsystem (settings, i18n, theme, input, audio, renderer,
 * caches, library, media, engines, launcher) and exposes NULL-safe
 * accessors to screens. All navigation lives in navigation.c; this file
 * is only wiring + frame scheduling.
 */

#include "app/app.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "log.h"
#include "i18n/locale.h"
#include "input/input.h"
#include "input/input_mapping.h"
#include "audio/audio.h"
#include "library/library.h"
#include "library/scanner.h"
#include "media/disc_manager.h"
#include "engine/engine_registry.h"
#include "engine/launcher.h"
#include "settings/settings_store.h"
#include "system/system_info.h"
#include "util/fs.h"
#include "util/path_util.h"
#include "util/string_util.h"
#include "ui/renderer.h"
#include "ui/layout.h"
#include "ui/toast.h"
#include "ui/font_cache.h"
#include "ui/texture_cache.h"
#include "ui/widgets.h"

#define APP_FRAME_MS 16        /* ~60fps cap when vsync is unavailable */

struct app {
    settings_model  settings;
    discadia_theme  theme;
    layout          layout;

    char data_dir[512];
    char cache_dir[512];
    char saves_dir[512];
    char games_dir[512];

    renderer        *ren;
    texture_cache   *tcache;
    font_cache      *fcache;
    audio           *aud;
    library         *lib;
    disc_manager    *discs;
    engine_registry *engines;
    toast_queue     *toasts;
    launcher         launch;

    widget_ctx       wctx;     /* rebuilt each draw                     */

    int   selected;            /* carousel logical index                */
    float carousel_pos;        /* animated position (float index)       */

    app_screen_t screen;
    app_screen_t prev_screen;  /* for fade transitions                  */
    Uint32       boot_time_ms;

    nav_request_t pending_nav; /* set by menu screen etc.               */
    int           launch_error;
    char          launch_title[256]; /* shown on launching screen       */

    int quit_requested;
};

/* ---- forward decls of vtables implemented across UI files ---- */
extern const screen_vtable *screen_boot_vtable(void);
extern const screen_vtable *screen_home_vtable(void);
extern const screen_vtable *screen_menu_vtable(void);
extern const screen_vtable *screen_settings_vtable(void);
extern const screen_vtable *screen_theme_selector_vtable(void);
extern const screen_vtable *screen_language_selector_vtable(void);
extern const screen_vtable *screen_input_remap_vtable(void);
extern const screen_vtable *screen_store_vtable(void);
extern const screen_vtable *screen_launching_vtable(void);

const screen_vtable *app_screen_table(app_screen_t s)
{
    switch (s)
    {
        case SCREEN_BOOT:             return screen_boot_vtable();
        case SCREEN_HOME:             return screen_home_vtable();
        case SCREEN_MENU:             return screen_menu_vtable();
        case SCREEN_SETTINGS:         return screen_settings_vtable();
        case SCREEN_THEME_SELECTOR:   return screen_theme_selector_vtable();
        case SCREEN_LANGUAGE_SELECTOR:return screen_language_selector_vtable();
        case SCREEN_INPUT_REMAP:      return screen_input_remap_vtable();
        case SCREEN_STORE:            return screen_store_vtable();
        case SCREEN_LAUNCHING:        return screen_launching_vtable();
        default:                      return screen_home_vtable();
    }
}

/* ---------------- paths ---------------- */

static void resolve_data_dir(app *a)
{
    const char *env = getenv("DISCADIA_DATA_DIR");
    if (env && *env && fs_dir_exists(env))
    {
        str_safe_copy(a->data_dir, env, sizeof(a->data_dir));
        return;
    }
    if (fs_dir_exists("/etc/discadia"))
    {
        str_safe_copy(a->data_dir, "/etc/discadia", sizeof(a->data_dir));
        return;
    }
    if (fs_dir_exists("./data"))
    {
        str_safe_copy(a->data_dir, "./data", sizeof(a->data_dir));
        return;
    }
    /* Nowhere else: run with built-in defaults (empty dir -> fallbacks). */
    a->data_dir[0] = '\0';
}

/* ---------------- create/init ---------------- */

app *app_create(void)
{
    app *a = calloc(1, sizeof(*a));
    if (!a) return NULL;
    settings_set_defaults(&a->settings);
    theme_set_defaults(&a->theme);
    a->screen = SCREEN_BOOT;
    a->prev_screen = SCREEN_BOOT;
    launcher_init(&a->launch);
    return a;
}

int app_init(app *a)
{
    if (!a) return -1;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER |
                 SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0)
    {
        LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return -1;
    }
    int img_flags = IMG_INIT_PNG | IMG_INIT_JPG;
    if ((IMG_Init(img_flags) & img_flags) == 0)
        LOG_INFO("SDL2_image unavailable (%s): covers use placeholders",
                 IMG_GetError());
    if (TTF_Init() != 0)
        LOG_INFO("SDL2_ttf unavailable (%s): text will be minimal",
                 TTF_GetError());
    atexit(TTF_Quit);

    resolve_data_dir(a);
    fs_get_cache_dir(a->cache_dir, sizeof(a->cache_dir));
    fs_get_saves_dir(a->saves_dir, sizeof(a->saves_dir));
    fs_get_data_dir(a->games_dir, sizeof(a->games_dir));
    fs_mkdir_p(a->cache_dir, 0755);
    fs_mkdir_p(a->games_dir, 0755);

    /* Settings first: they pick theme + language. */
    settings_store_load(&a->settings);

    if (locale_init(a->data_dir, a->settings.language) != 0)
        LOG_WARN("no locales found under %s: raw keys will be shown",
                 a->data_dir);

    if (theme_load(&a->theme, a->data_dir, a->settings.theme) != 0)
    {
        theme_load(&a->theme, a->data_dir, "arcade-classic");
    }
    /* If the requested theme silently fell back, tell the user once. */
    char probe[512];
    path_join(probe, sizeof(probe), a->data_dir, "themes");
    path_join(probe, sizeof(probe), probe, a->settings.theme);
    if (!fs_dir_exists(probe) && strcmp(a->settings.theme, "arcade-classic"))
        LOG_WARN("theme '%s' not found, using defaults", a->settings.theme);

    toast_set_default_ttl_ms(a->theme.toast_timeout_ms);

    /* Input mapping before opening controllers. */
    input_init();
    input_mapping_load();

    a->ren = renderer_create(1, getenv("DISCADIA_VIDEODRIVER"));
    if (!a->ren) return -1;

    int sw, sh;
    renderer_size(a->ren, &sw, &sh);
    layout_compute(&a->layout, &a->theme, sw, sh);

    a->tcache = texture_cache_create(renderer_sdl(a->ren));
    a->fcache = font_cache_create();

    /* Register faces: theme fonts with system fallbacks. */
    const char *fb1 = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf";
    const char *fb2 = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    font_cache_add_face(a->fcache, FACE_TITLE, a->theme.font_paths.title_font,
                        a->theme.sizes.title_size, fb1);
    font_cache_add_face(a->fcache, FACE_BODY, a->theme.font_paths.body_font,
                        a->theme.sizes.description_size, fb2);
    font_cache_add_face(a->fcache, FACE_SMALL, a->theme.font_paths.small_font,
                        a->theme.sizes.meta_size, fb2);

    a->aud = audio_create(44100);
    audio_set_enabled(a->aud, a->settings.audio_enabled);
    audio_set_volume(a->aud, a->settings.audio_volume);
    app_reload_audio_sounds(a);

    a->toasts = toast_queue_create();
    a->lib = library_create();
    a->engines = engine_registry_create();
    char edir[512];
    path_join(edir, sizeof(edir), a->data_dir, "engines");
    int eng_n = engine_registry_load_dir(a->engines, edir);
    LOG_INFO("engines loaded: %d", eng_n);

    const char *media_root = getenv("DISCADIA_MEDIA_DIR");
    a->discs = disc_manager_create(media_root && *media_root ? media_root
                                                             : "/media/discadia");

    scanner_scan_installed(a->lib);
    disc_manager_update(a->discs, a->lib, SDL_GetTicks());
    library_sort(a->lib, &a->selected);
    if (a->selected < 0 || a->selected >= library_count(a->lib))
        a->selected = 0;
    a->carousel_pos = (float)a->selected;

    a->boot_time_ms = SDL_GetTicks();
    LOG_INFO("Discadia %s ready on %s (%s, %ld MB RAM)",
             DISCADIA_VERSION, sysinfo_model(), sysinfo_arch(),
             sysinfo_total_ram_mb());
    return 0;
}

void app_reload_audio_sounds(app *a)
{
    if (!a || !a->aud) return;
    const char *paths[THEME_MAX_SOUNDS];
    const char *names[THEME_MAX_SOUNDS];
    int n = a->theme.sounds.sound_count;
    if (n > THEME_MAX_SOUNDS) n = THEME_MAX_SOUNDS;
    for (int i = 0; i < n; i++)
    {
        names[i] = a->theme.sounds.sound_names[i];
        paths[i] = a->theme.sounds.sound_paths[i];
    }
    audio_reload_sounds(a->aud, paths, names, n);
}

/* ---------------- shutdown/destroy ---------------- */

void app_shutdown(app *a)
{
    if (!a) return;
    settings_store_save(&a->settings);
    input_mapping_save();
    LOG_INFO("settings saved, shutting down");
}

void app_destroy(app *a)
{
    if (!a) return;
    toast_queue_destroy(a->toasts);
    disc_manager_destroy(a->discs);
    engine_registry_destroy(a->engines);
    library_destroy(a->lib);
    audio_destroy(a->aud);
    font_cache_destroy(a->fcache);
    texture_cache_destroy(a->tcache);
    renderer_destroy(a->ren);
    input_shutdown();
    IMG_Quit();
    SDL_Quit();
    free(a);
}

/* ---------------- accessors ---------------- */

void app_set_screen(app *a, app_screen_t s)
{
    if (!a || s == a->screen || s >= SCREEN_COUNT) return;
    const screen_vtable *old = app_screen_table(a->screen);
    if (old && old->leave) old->leave(a);
    a->prev_screen = a->screen;
    a->screen = s;
    app_state_begin_fade(a);
    const screen_vtable *nw = app_screen_table(s);
    if (nw && nw->enter) nw->enter(a);
}

app_screen_t app_current_screen(const app *a)
{
    return a ? a->screen : SCREEN_HOME;
}

const char *app_data_dir(const app *a)  { return a ? a->data_dir : ""; }
const char *app_cache_dir(const app *a) { return a ? a->cache_dir : ""; }

engine_registry *app_engines(const app *a) { return a ? a->engines : NULL; }
toast_queue *app_toasts(const app *a)      { return a ? a->toasts : NULL; }
audio *app_audio_sys(const app *a)         { return a ? a->aud : NULL; }
disc_manager *app_discs(const app *a)      { return a ? a->discs : NULL; }
library *app_library(const app *a)         { return a ? a->lib : NULL; }
renderer *app_renderer(const app *a)       { return a ? a->ren : NULL; }
texture_cache *app_texcache(const app *a)  { return a ? a->tcache : NULL; }
font_cache *app_fontcache(const app *a)    { return a ? a->fcache : NULL; }
settings_model *app_settings(app *a)       { return a ? &a->settings : NULL; }
discadia_theme *app_theme(app *a)          { return a ? &a->theme : NULL; }
layout *app_layout(app *a)                 { return a ? &a->layout : NULL; }
launcher *app_launcher(app *a)             { return a ? &a->launch : NULL; }
const char *app_saves_base(app *a)         { return a ? a->saves_dir : "/tmp"; }
const char *app_version(void)              { return DISCADIA_VERSION; }

float app_carousel_pos(const app *a)  { return a ? a->carousel_pos : 0.f; }
void  app_set_carousel_pos(app *a, float p) { if (a) a->carousel_pos = p; }
int   app_selected_item(const app *a) { return a ? a->selected : 0; }
void  app_set_selected(app *a, int idx)
{
    if (!a) return;
    int n = library_count(a->lib);
    if (n <= 0) { a->selected = 0; a->carousel_pos = 0.f; return; }
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    a->selected = idx;
}
int app_launch_error(const app *a) { return a ? a->launch_error : 0; }
void app_set_launch_error(app *a, int e) { if (a) a->launch_error = e; }
const char *app_launch_title(app *a) { return a ? a->launch_title : ""; }
void app_set_launch_title(app *a, const char *t)
{
    if (a && t) str_safe_copy(a->launch_title, t, sizeof(a->launch_title));
}

nav_request_t app_take_pending_nav(app *a)
{
    if (!a) return NAV_NONE;
    nav_request_t n = a->pending_nav;
    a->pending_nav = NAV_NONE;
    return n;
}

void app_request_nav(app *a, nav_request_t n)
{
    if (a) a->pending_nav = n;
}

void app_quit(app *a)
{
    if (a) a->quit_requested = 1;
}

void app_play_sound(app *a, const char *logical_name)
{
    if (a && a->aud && a->theme.sounds.enabled)
        audio_play(a->aud, logical_name);
}

void app_rescan_library(app *a)
{
    if (!a) return;
    int keep_sel = a->selected;
    library_clear(a->lib);
    scanner_scan_installed(a->lib);
    disc_manager_update(a->discs, a->lib, SDL_GetTicks() + 1000);
    library_sort(a->lib, &keep_sel);
    a->selected = (keep_sel >= 0 && keep_sel < library_count(a->lib))
                      ? keep_sel : 0;
    a->carousel_pos = (float)a->selected;
}

void app_reapply_theme(app *a)
{
    if (!a) return;
    theme_load(&a->theme, a->data_dir, a->settings.theme);
    toast_set_default_ttl_ms(a->theme.toast_timeout_ms);
    int sw, sh;
    renderer_size(a->ren, &sw, &sh);
    layout_compute(&a->layout, &a->theme, sw, sh);
    font_cache_flush(a->fcache);
    font_cache_add_face(a->fcache, FACE_TITLE, a->theme.font_paths.title_font,
                        a->theme.sizes.title_size,
                        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf");
    font_cache_add_face(a->fcache, FACE_BODY, a->theme.font_paths.body_font,
                        a->theme.sizes.description_size,
                        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    font_cache_add_face(a->fcache, FACE_SMALL, a->theme.font_paths.small_font,
                        a->theme.sizes.meta_size,
                        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    audio_set_enabled(a->aud, a->settings.audio_enabled &&
                                a->theme.sounds.enabled);
    audio_set_volume(a->aud, a->settings.audio_volume);
    app_reload_audio_sounds(a);
}

widget_ctx app_widget_ctx(app *a)
{
    widget_ctx w = {0};
    if (a)
    {
        w.ren = a->ren;
        w.fcache = a->fcache;
        w.tcache = a->tcache;
        w.theme = &a->theme;
    }
    return w;
}

/* ---------------- main loop ---------------- */

void app_run(app *a)
{
    if (!a || !a->ren) return;
    const screen_vtable *cur = app_screen_table(a->screen);
    if (cur && cur->enter) cur->enter(a);

    Uint32 last = SDL_GetTicks();
    while (!a->quit_requested)
    {
        Uint32 now = SDL_GetTicks();
        float dt = (float)(now - last);
        if (dt < 1.f) dt = 1.f;
        if (dt > 100.f) dt = 100.f;   /* clamp after pauses */
        last = now;

        input_poll();

        /* Global hot events. */
        int hp = input_pop_hotplug_event();
        if (hp != 0)
        {
            toast_push(app_toasts(a),
                       locale_get(hp > 0 ? "controller_connected"
                                         : "controller_disconnected"));
        }
        if (input_window_close_requested()) a->quit_requested = 1;

        /* Media polling (once per second inside the manager). */
        int media_ev = disc_manager_update(a->discs, a->lib, now);
        if (media_ev & 1)
        {
            const library_item *ni = library_get(a->lib, 0);
            char msg[256];
            locale_fmt(msg, sizeof(msg), "disc_inserted", "title",
                       ni ? ni->title : "", NULL, NULL);
            toast_push(app_toasts(a), msg);
            app_play_sound(a, "cd_insert");
        }
        if (media_ev & 2) toast_push(app_toasts(a), locale_get("disc_removed"));
        if (media_ev & 4) toast_push_error(app_toasts(a),
                                    locale_get("error_disc_unrecognized"));

        /* Build this frame's input snapshot. */
        frame_input fi = {0};
        fi.left     = input_get(IA_LEFT).pressed;
        fi.right    = input_get(IA_RIGHT).pressed;
        fi.up       = input_get(IA_UP).pressed;
        fi.down     = input_get(IA_DOWN).pressed;
        fi.confirm  = input_get(IA_CONFIRM).pressed;
        fi.back     = input_get(IA_BACK).pressed;
        fi.menu     = input_get(IA_MENU).pressed;
        fi.context  = input_get(IA_CONTEXT).pressed;
        fi.any_confirm_or_back = input_any_pressed();

        /* Screen update -> navigation requests. */
        nav_request_t nav = NAV_NONE;
        cur = app_screen_table(a->screen);
        if (cur && cur->update) cur->update(a, dt, &fi, &nav);

        /* Menu-driven cross-screen requests take priority. */
        nav_request_t pend = app_take_pending_nav(a);
        if (pend != NAV_NONE) nav = pend;

        app_handle_navigation(a, nav);
        app_state_update_fade(dt);

        /* Toast timers + render. */
        toast_update(app_toasts(a), dt);

        a->wctx = app_widget_ctx(a);
        cur = app_screen_table(a->screen);
        if (cur && cur->draw) cur->draw(a, dt);

        /* Transition fade-to-black overlay. */
        float fa = app_state_fade_alpha();
        if (fa > 0.f)
        {
            SDL_SetRenderDrawColor(renderer_sdl(a->ren), 0, 0, 0,
                                   (Uint8)(fa * 160.f));
            SDL_Rect full = {0, 0, a->layout.sw, a->layout.sh};
            SDL_RenderFillRect(renderer_sdl(a->ren), &full);
        }

        toast_draw(a->toasts, &a->wctx, a->layout.sw);
        renderer_present(a->ren);

        Uint32 spent = SDL_GetTicks() - now;
        if (spent < APP_FRAME_MS) SDL_Delay(APP_FRAME_MS - spent);
    }
}
