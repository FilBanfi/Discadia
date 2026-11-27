/*
 * store_screen.c - the Discadia Store UI.
 *
 * State machine (kept dead simple, no threads: fetches are fast JSON
 * and installs show a blocking modal with a spinner):
 *
 *   ST_LOADING  -> fetching/parsing index.json (cache-first when fresh)
 *   ST_LIST     -> vertical package list, scrollable, focus ring
 *   ST_DETAIL   -> description + actions (Install / Update / Open)
 *   ST_BUSY     -> modal overlay while downloading/installing
 *   ST_ERROR    -> message + retry/back
 *
 * Index caching: the raw JSON is stored under the cache dir; if it is
 * younger than settings.store_cache_days we parse the cache instead of
 * hitting the network (offline-friendly). "Check for Updates" forces a
 * refetch and flags packages whose remote version differs from the
 * local install marker.
 */

#include "store/store_screen.h"
#include "store/store_client.h"
#include "store/store_index.h"
#include "store/compatibility.h"
#include "store/package_installer.h"
#include "app/app.h"
#include "ui/renderer.h"
#include "ui/widgets.h"
#include "ui/animation.h"
#include "ui/top_bar.h"
#include "ui/toast.h"
#include "i18n/locale.h"
#include "library/library.h"
#include "library/scanner.h"
#include "engine/engine_registry.h"
#include "util/fs.h"
#include "util/path_util.h"
#include "util/string_util.h"
#include "log.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <math.h>

typedef enum {
    ST_LOADING = 0,
    ST_LIST,
    ST_DETAIL,
    ST_BUSY,
    ST_ERROR
} store_state_t;

#define LIST_VISIBLE 8      /* rows on screen at once */

/* The screen vtable callbacks all receive `app *`; we stash it on enter
 * so helpers like start_fetch (called from enter) can reach services. */
static app *g_app = NULL;

static struct {
    store_state_t state;
    store_model   model;
    int           sel;          /* focused row in list            */
    int           scroll;       /* first visible row              */
    int           action;       /* detail page: 0=primary 1=back  */
    char          error[256];
    float         spin_age_ms;  /* busy spinner animation clock   */
    int           loaded_once;  /* avoid re-fetch on every enter  */
    char          busy_msg[256];
} s;  /* single static instance: the store is a singleton screen */

/* ---- index loading ---------------------------------------------------- */

static void refresh_installed_flags(void)
{
    for (int i = 0; i < s.model.count; i++) {
        store_pkg *p = &s.model.items[i];
        char cur[32] = "";
        p->installed = installer_is_installed(p, cur, sizeof(cur));
        p->update_available = (p->installed && cur[0] && p->version[0] &&
                               strcmp(cur, p->version) != 0);
        p->compatible = compat_check_pkg(p);
    }
}

static int load_from_cache(const char *cache_file)
{
    if (!fs_file_exists(cache_file)) return -1;
    if (store_index_parse_file(cache_file, &s.model) != 0) return -1;
    s.model.from_cache = 1;
    return 0;
}

static void start_fetch(int force_refetch)
{
    const char *base = store_base_url();
    char cache_file[768];
    path_join(cache_file, sizeof(cache_file), app_cache_dir(g_app),
              "store-index.json");

    /* Cache freshness window from settings (days -> seconds). */
    int days = app_settings(g_app)->store_cache_days;
    if (days < 0) days = 0;
    struct stat st;
    if (!force_refetch && stat(cache_file, &st) == 0) {
        long long age = (long long)time(NULL) - (long long)st.st_mtime;
        if (age < (long long)days * 86400LL && load_from_cache(cache_file) == 0)
        {
            refresh_installed_flags();
            s.state = ST_LIST;
            LOG_INFO("store index served from cache (%lld s old)", age);
            return;
        }
    }

    /* Network fetch. Blocking here is acceptable: it happens once per
     * session on store open and curl has --max-time 30. */
    char url[SC_URL_LEN];
    store_join_url(url, sizeof(url), base, "index.json");
    char *text = store_http_get(url, 512 * 1024);
    if (!text) {
        /* Offline? Fall back to stale cache rather than an error wall. */
        if (load_from_cache(cache_file) == 0) {
            refresh_installed_flags();
            s.state = ST_LIST;
            toast_push(app_toasts(g_app),
                       locale_get("store_offline_cache"));
            return;
        }
        str_safe_copy(s.error, locale_get("error_network"), sizeof(s.error));
        s.state = ST_ERROR;
        return;
    }

    if (store_index_parse(text, &s.model) != 0) {
        free(text);
        str_safe_copy(s.error, locale_get("error_manifest"), sizeof(s.error));
        s.state = ST_ERROR;
        return;
    }
    fs_write_file(cache_file, text, strlen(text)); /* best-effort cache */
    free(text);
    s.model.from_cache = 0;
    refresh_installed_flags();
    s.state = ST_LIST;
}

/* ---- screen vtable ----------------------------------------------------- */

static void store_enter(app *a)
{
    g_app = a;
    if (s.state == ST_LOADING && !s.loaded_once) {
        s.loaded_once = 1;
        start_fetch(app_settings(a)->store_auto_update_index);
    } else if (s.state == ST_ERROR) {
        s.state = ST_LOADING;
        start_fetch(1);
    }
}

static void store_leave(app *a) { (void)a; }

static void do_install(app *a, store_pkg *p)
{
    str_format(s.busy_msg, sizeof(s.busy_msg), "%s: %s",
               locale_get("install"), p->name);
    s.state = ST_BUSY;
    s.spin_age_ms = 0.f;

    char detail[256];
    install_result_t r = installer_install(p, store_base_url(),
                                           app_cache_dir(a),
                                           detail, sizeof(detail));
    if (r == INSTALL_OK) {
        char msg[320];
        locale_fmt(msg, sizeof(msg), "install_complete", "name", p->name,
                   NULL, NULL);
        toast_push(app_toasts(a), msg);
        app_play_sound(a, "select");

        /* Post-install refresh depends on package type. */
        switch (p->type) {
        case PKG_GAME: case PKG_APP:
            app_rescan_library(a);
            break;
        case PKG_ENGINE:
            {
                char edir[512];
                const char *dd = app_data_dir(a);
                if (dd && dd[0]) path_join(edir, sizeof(edir), dd, "engines");
                else str_safe_copy(edir, "", sizeof(edir));
                engine_registry_load_dir(app_engines(a), edir);
            }
            break;
        case PKG_THEME:
            /* New theme appears in the selector next time it lists;
             * nothing global to reload unless it is applied. */
            break;
        default: break;
        }
        refresh_installed_flags();
        s.state = ST_DETAIL;
    } else {
        char key_buf[64];
        str_safe_copy(key_buf, install_result_locale_key(r), sizeof(key_buf));
        char msg[320];
        locale_fmt(msg, sizeof(msg), "error_install_failed", "reason",
                   locale_get(key_buf), NULL, NULL);
        toast_push_error(app_toasts(a), msg);
        s.state = ST_DETAIL;
    }
}

static void store_update(app *a, float dt_ms, const frame_input *in,
                         nav_request_t *out_nav)
{
    (void)out_nav;
    s.spin_age_ms += dt_ms;

    if (in->back) {
        if (s.state == ST_DETAIL || s.state == ST_ERROR) {
            s.state = ST_LIST;
            app_play_sound(a, "back");
        } else if (s.state == ST_LIST) {
            *out_nav = NAV_BACK;
        }
        return;
    }
    if (s.state == ST_BUSY) return; /* modal: swallow everything else */

    if (s.state == ST_ERROR) {
        if (in->confirm) { s.state = ST_LOADING; start_fetch(1); }
        return;
    }
    if (s.state == ST_LOADING) return; /* fetch already completed in enter */

    if (s.state == ST_LIST) {
        int n = s.model.count;
        if (n <= 0) { if (in->confirm) *out_nav = NAV_BACK; return; }

        if (in->up)   { s.sel = (s.sel + n - 1) % n; app_play_sound(a, "navigate"); }
        if (in->down) { s.sel = (s.sel + 1) % n;     app_play_sound(a, "navigate"); }
        /* keep selection inside the visible window */
        if (s.sel < s.scroll) s.scroll = s.sel;
        if (s.sel >= s.scroll + LIST_VISIBLE) s.scroll = s.sel - LIST_VISIBLE + 1;

        if (in->confirm) { s.action = 0; s.state = ST_DETAIL;
                           app_play_sound(a, "select"); }
        return;
    }

    if (s.state == ST_DETAIL) {
        store_pkg *p = &s.model.items[s.sel];
        if (in->up || in->down) {
            s.action = !s.action;
            app_play_sound(a, "navigate");
        }
        if (in->confirm) {
            if (s.action == 0) {
                if (!p->compatible) {
                    toast_push_error(app_toasts(a),
                                     locale_get("error_incompatible"));
                } else {
                    do_install(a, p);
                }
            } else {
                s.state = ST_LIST;
                app_play_sound(a, "back");
            }
        }
    }
}

/* ---- drawing ------------------------------------------------------------ */

static void draw_list(app *a)
{
    widget_ctx w = app_widget_ctx(a);
    layout *L = app_layout(a);
    int x = L->sw / 6, wp = L->sw - 2 * x;
    int y = L->top_bar.y + L->top_bar.h + 20;
    int rh = (L->sh - y - 40) / LIST_VISIBLE;

    if (s.model.count == 0) {
        widget_text_primary(&w, FACE_BODY, app_theme(a)->sizes.description_size,
                            locale_get("empty_store"),
                            L->sw / 2, L->sh / 2, 0, L->sw, 1);
        return;
    }

    for (int vis = 0; vis < LIST_VISIBLE; vis++) {
        int idx = s.scroll + vis;
        if (idx >= s.model.count) break;
        store_pkg *p = &s.model.items[idx];
        int ry = y + vis * rh;
        int sel = (idx == s.sel);

        widget_list_row(&w, x, ry, wp, rh - 6, p->name, "", sel);

        /* Right-side badge: installed / update / incompatible / size. */
        char badge[64];
        if (!p->compatible)        str_safe_copy(badge, locale_get("store_incompatible"), sizeof(badge));
        else if (p->update_available) str_safe_copy(badge, locale_get("update"), sizeof(badge));
        else if (p->installed)     str_safe_copy(badge, locale_get("installed"), sizeof(badge));
        else { str_format(badge, sizeof(badge), "%ld MB", p->size_mb); }

        SDL_Color c = theme_color(app_theme(a), TCOL_TEXT_SECONDARY);
        if (p->update_available) c = theme_color(app_theme(a), TCOL_ACCENT);
        if (!p->compatible)      c = theme_color(app_theme(a), TCOL_SECONDARY);
        widget_text(&w, FACE_SMALL, app_theme(a)->sizes.meta_size, c, badge,
                    x + wp - 16, ry + rh / 2 - app_theme(a)->sizes.meta_size / 2, 1, 200, 1);

        /* Type tag left of badge. */
        widget_text(&w, FACE_SMALL, app_theme(a)->sizes.meta_size,
                    theme_color(app_theme(a), TCOL_TEXT_SECONDARY),
                    locale_get(pkg_type_locale_key(p->type)),
                    x + wp - 220, ry + rh / 2 - app_theme(a)->sizes.meta_size / 2, 1, 160, 1);
    }
}

static void draw_detail(app *a)
{
    widget_ctx w = app_widget_ctx(a);
    layout *L = app_layout(a);
    store_pkg *p = &s.model.items[s.sel];

    int bw = L->sw * 2 / 3, bh = L->sh / 2;
    int bx = (L->sw - bw) / 2, by = (L->sh - bh) / 2;
    renderer_fill_rounded(app_renderer(a), bx, by, bw, bh, 12,
                          theme_color(app_theme(a), TCOL_TOAST_BG), 245);
    renderer_outline_rounded(app_renderer(a), bx, by, bw, bh, 12,
                             theme_color(app_theme(a), TCOL_TOAST_BORDER), 2);

    widget_text_primary(&w, FACE_TITLE, app_theme(a)->sizes.title_size, p->name,
                        bx + 24, by + 18, -1, bw - 48, 1);

    char meta[128];
    str_format(meta, sizeof(meta), "%s \xC2\xB7 v%s \xC2\xB7 %ld MB",
               locale_get(pkg_type_locale_key(p->type)), p->version,
               p->size_mb);
    widget_text_secondary(&w, FACE_SMALL, app_theme(a)->sizes.meta_size, meta,
                          bx + 24, by + 18 + app_theme(a)->sizes.title_size + 6, -1,
                          bw - 48, 1);

    widget_text(&w, FACE_BODY, app_theme(a)->sizes.description_size,
                theme_color(app_theme(a), TCOL_TEXT),
                p->description[0] ? p->description : locale_get("no_description"),
                bx + 24, by + bh / 2 - 30, -1, bw - 48, 4);

    /* Action rows. */
    const char *primary =
        !p->compatible          ? locale_get("store_incompatible") :
        p->update_available     ? locale_get("update") :
        p->installed            ? locale_get("reinstall") :
                                  locale_get("install");
    widget_list_row(&w, bx + 24, by + bh - 2 * (bh / 6) - 12,
                    bw - 48, bh / 6, primary, "", s.action == 0);
    widget_list_row(&w, bx + 24, by + bh - (bh / 6) - 6,
                    bw - 48, bh / 6, locale_get("back"), "", s.action == 1);
}

static void draw_busy(app *a)
{
    widget_ctx w = app_widget_ctx(a);
    layout *L = app_layout(a);

    /* Dim layer over the whole screen. Animations respect the global
     * kill-switch: without them the spinner just stays static. */
    renderer_fill_rounded(app_renderer(a), 0, 0, L->sw, L->sh, 0,
                          (SDL_Color){0, 0, 0, 150}, 150);

    int bw = L->sw / 2, bh = L->sh / 5;
    int bx = (L->sw - bw) / 2, by = (L->sh - bh) / 2;
    renderer_fill_rounded(app_renderer(a), bx, by, bw, bh, 12,
                          theme_color(app_theme(a), TCOL_TOAST_BG), 250);
    renderer_outline_rounded(app_renderer(a), bx, by, bw, bh, 12,
                             theme_color(app_theme(a), TCOL_ACCENT), 2);

    widget_text_primary(&w, FACE_BODY, app_theme(a)->sizes.description_size, s.busy_msg,
                        L->sw / 2, by + bh / 4, 0, bw - 20, 1);

    /* Spinner: 12 ticks rotating via the pingpong/triangle helper. */
    int anims_on = app_settings(a)->animations_enabled &&
                   app_theme(a)->animations.enabled;
    float phase = anims_on ? (anim_clamp((s.spin_age_ms / 900.f), 0, 1)) : 0.f;
    SDL_Color acc = theme_color(app_theme(a), TCOL_ACCENT);
    for (int i = 0; i < 12; i++) {
        int lit = ((i + (int)(phase * 12)) % 12) < 3;
        double ang = i * (3.14159265 / 6.0);
        int cx = L->sw / 2 + (int)(30 * cos(ang));
        int cy = by + bh * 3 / 4 + (int)(14 * sin(ang));
        renderer_fill_rounded(app_renderer(a), cx - 2, cy - 2, 5, 5, 2,
                              lit ? acc : (SDL_Color){acc.r, acc.g, acc.b, 60},
                              lit ? 255 : 60);
    }
}

static void draw_loading(app *a)
{
    widget_ctx w = app_widget_ctx(a);
    layout *L = app_layout(a);
    widget_text_primary(&w, FACE_BODY, app_theme(a)->sizes.description_size,
                        locale_get("store_loading"),
                        L->sw / 2, L->sh / 2, 0, L->sw, 1);
    /* pulsing dots use the same global animations gate */
    int anims_on = app_settings(a)->animations_enabled &&
                   app_theme(a)->animations.enabled;
    float t = anims_on ? anim_pingpong_ms(s.spin_age_ms, 1200.f) : 0.5f;
    SDL_Color c = theme_color(app_theme(a), TCOL_ACCENT);
    c.a = (Uint8)(80 + t * 175);
    for (int i = 0; i < 3; i++)
        renderer_fill_rounded(app_renderer(a),
                              L->sw / 2 - 24 + i * 20, L->sh / 2 + 30,
                              10, 10, 5, c, 255);
}

static void draw_error(app *a)
{
    widget_ctx w = app_widget_ctx(a);
    layout *L = app_layout(a);
    widget_text_primary(&w, FACE_BODY, app_theme(a)->sizes.description_size,
                        locale_get("store_error_title"),
                        L->sw / 2, L->sh / 2 - 30, 0, L->sw, 1);
    widget_text_secondary(&w, FACE_SMALL, app_theme(a)->sizes.meta_size, s.error,
                          L->sw / 2, L->sh / 2, 0, L->sw, 2);
    widget_text_secondary(&w, FACE_SMALL, app_theme(a)->sizes.meta_size,
                          locale_get("press_confirm_retry"),
                          L->sw / 2, L->sh / 2 + 40, 0, L->sw, 1);
}

static void store_draw(app *a, float dt_ms)
{
    (void)dt_ms;
    layout *L = app_layout(a);
    renderer_draw_background(app_renderer(a), app_theme(a));
    top_bar_draw(a, locale_get("store"));

    s.spin_age_ms += 0; /* advanced in update */

    switch (s.state) {
    case ST_LOADING: draw_loading(a); break;
    case ST_LIST:    draw_list(a);    break;
    case ST_DETAIL:  draw_list(a); draw_detail(a); break;
    case ST_BUSY:    draw_list(a); draw_busy(a);   break;
    case ST_ERROR:   draw_error(a);   break;
    }
    (void)L;
}

const screen_vtable *screen_store_vtable(void)
{
    static const screen_vtable vt = {
        .name = "store",
        .enter = store_enter,
        .update = store_update,
        .draw = store_draw,
        .leave = store_leave,
    };
    return &vt;
}
