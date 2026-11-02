/*
 * store_screen.c - graphical store over the router repository index.
 *
 * States: LOADING (fetch/cache index) -> LIST (carousel of packages,
 * keyboard typing filters the list) -> DETAIL (description, size,
 * compatibility, Install/Update action) -> INSTALLING (blocking modal;
 * network operations are seconds-long so they run between update and
 * draw with a spinner frame). "Check for Updates" is an explicit list
 * entry that refetches the index and flags newer remote versions;
 * applying updates reinstalls packages only - user settings in
 * ~/.config/discadia are never touched.
 */

#include <stdio.h>
#include "ui/top_bar.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <SDL.h>
#include "store/store_screen.h"
#include "store/store_index.h"
#include "store/store_client.h"
#include "store/package_installer.h"
#include "store/compatibility.h"
#include "app/app.h"
#include "audio/audio.h"
#include "ui/widgets.h"
#include "ui/renderer.h"
#include "ui/toast.h"
#include "i18n/locale.h"
#include "system/network_status.h"
#include "util/fs.h"
#include "util/path_util.h"
#include "util/string_util.h"
#include "log.h"

typedef enum st_state { ST_LOADING, ST_LIST, ST_DETAIL, ST_INSTALLING } st_state_t;

#define FILTER_LEN 64

static store_model s_model;
static st_state_t  s_state = ST_LOADING;
static int         s_sel = 0;        /* list selection   */
static int         s_scroll = 0;
static int         s_detail_action = 0;  /* 0 install/update, 1 back */
static char        s_filter[FILTER_LEN] = "";
static char        s_cache_path[512] = "";
static int         s_load_attempted = 0;

/* Indices into s_model filtered by s_filter (case-insensitive on name). */
static int  s_view[ST_MAX_PACKAGES];
static int  s_view_count = 0;

static int name_matches(const char *name, const char *needle)
{
    /* ASCII case-insensitive substring search (titles are latin). */
    size_t nl = strlen(needle);
    if (!nl) return 1;
    for (const char *h = name; *h; h++)
    {
        const char *a = h, *b = needle;
        while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b))
        { a++; b++; }
        if ((size_t)(b - needle) == nl) return 1;
    }
    return 0;
}

static void rebuild_view(void)
{
    s_view_count = 0;
    for (int i = 0; i < s_model.count; i++)
    {
        if (name_matches(s_model.items[i].name, s_filter))
            s_view[s_view_count++] = i;
    }
    if (s_sel >= s_view_count) s_sel = s_view_count ? s_view_count - 1 : 0;
    if (s_scroll > s_sel) s_scroll = s_sel;
}

static void refresh_installed_flags(app *a)
{
    for (int i = 0; i < s_model.count; i++)
    {
        char cur[32] = "";
        s_model.items[i].installed =
            installer_is_installed(&s_model.items[i], cur, sizeof(cur));
        s_model.items[i].update_available =
            s_model.items[i].installed && cur[0] &&
            strcmp(cur, s_model.items[i].version) != 0;
        s_model.items[i].compatible = compat_check_pkg(&s_model.items[i]);
    }
    (void)a;
}

static int count_updates(void)
{
    int n = 0;
    for (int i = 0; i < s_model.count; i++)
        if (s_model.items[i].update_available) n++;
    return n;
}

/* Try cache first when offline or young enough; else fetch remotely. */
static void load_index(app *a)
{
    char cache_dir[512];
    snprintf(cache_dir, sizeof(cache_dir), "%s/store", app_cache_dir(a));
    fs_mkdir_p(cache_dir, 0755);
    snprintf(s_cache_path, sizeof(s_cache_path), "%s/index.json", cache_dir);

    int loaded = 0;
    int online = network_is_online();

    if (online && app_settings(a)->store_auto_update_index)
    {
        char url[SC_URL_LEN];
        store_join_url(url, sizeof(url), store_base_url(), "index.json");
        char *text = store_http_get(url, 4u * 1024 * 1024);
        if (text)
        {
            if (store_index_parse(text, &s_model) == 0)
            {
                fs_write_file_atomic(s_cache_path, text, strlen(text));
                loaded = 1;
            }
            free(text);
        }
    }
    if (!loaded && fs_file_exists(s_cache_path))
    {
        if (store_index_parse_file(s_cache_path, &s_model) == 0)
        {
            loaded = 1;
            s_model.from_cache = 1;
            if (!online)
                toast_push(app_toasts(a), locale_get("offline_mode"));
        }
    }
    if (!loaded)
    {
        store_model_clear(&s_model);
        toast_push_error(app_toasts(a),
                         locale_get(online ? "error_network"
                                           : "error_no_connection"));
    }
    refresh_installed_flags(a);
    rebuild_view();
    s_state = ST_LIST;
}

static void start_install(app *a, store_pkg *pkg)
{
    char cache_dir[512];
    snprintf(cache_dir, sizeof(cache_dir), "%s/store", app_cache_dir(a));

    char detail[200] = "";
    install_result_t r = installer_install(pkg, store_base_url(), cache_dir,
                                           detail, sizeof(detail));
    if (r == INSTALL_OK)
    {
        char msg[256];
        str_format(msg, sizeof(msg), "%s (%s)", locale_get("install_complete"),
                   pkg->name);
        toast_push(app_toasts(a), msg);
        app_play_sound(a, "select");
        pkg->installed = 1;
        pkg->update_available = 0;
        app_rescan_library(a);
        app_reapply_theme(a);   /* picks up newly installed fonts/sounds */
    }
    else
    {
        char msg[256];
        str_format(msg, sizeof(msg), "%s %s", locale_get("error_install_failed"),
                   locale_get(install_result_locale_key(r)));
        toast_push_error(app_toasts(a), msg);
    }
    s_state = ST_DETAIL;
}

static void ss_enter(app *a)
{
    s_state = ST_LOADING;
    s_sel = 0; s_scroll = 0; s_filter[0] = '\0';
    s_load_attempted = 0;
    store_model_clear(&s_model);
}

static void ss_leave(app *a) { (void)a; }

static void ss_update(app *a, float dt_ms, const frame_input *in,
                      nav_request_t *out_nav)
{
    (void)dt_ms;

    if (s_state == ST_LOADING)
    {
        /* Defer the blocking fetch one frame so the loading UI paints. */
        if (++s_load_attempted >= 2) load_index(a);
        return;
    }

    if (s_state == ST_INSTALLING) { *out_nav = NAV_NONE; return; }

    if (in->back)
    {
        if (s_state == ST_DETAIL) { s_state = ST_LIST; app_play_sound(a, "back"); }
        else *out_nav = NAV_BACK;
        return;
    }

    if (s_state == ST_LIST)
    {
        /* Keyboard search: printable chars append to the filter. */
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_BACKSPACE &&
                s_filter[0])
                s_filter[strlen(s_filter) - 1] = '\0';
            else if (ev.type == SDL_TEXTINPUT &&
                     (ev.text.text[0] >= ' ' && ev.text.text[0] <= '~'))
            {
                size_t l = strlen(s_filter);
                if (l < FILTER_LEN - 1)
                {
                    s_filter[l] = ev.text.text[0];
                    s_filter[l + 1] = '\0';
                }
            }
        }
        rebuild_view();

        if (in->left  && s_sel > 0) { s_sel--; app_play_sound(a, "navigate"); }
        if (in->right && s_sel < s_view_count) { s_sel++; app_play_sound(a, "navigate"); }
        if (in->up || in->down) { /* list is horizontal like the carousel */ }

        if (in->context)   /* SELECT: check for updates / refresh index */
        {
            s_state = ST_LOADING;
            s_load_attempted = 0;
            app_settings(a)->store_auto_update_index = 1;
            app_play_sound(a, "select");
            return;
        }

        if (in->confirm && s_view_count > 0)
        {
            s_state = ST_DETAIL;
            s_detail_action = 0;
            app_play_sound(a, "select");
        }
        int visible = 9;
        if (s_sel < s_scroll) s_scroll = s_sel;
        if (s_sel >= s_scroll + visible) s_scroll = s_sel - visible + 1;
        return;
    }

    if (s_state == ST_DETAIL)
    {
        if (s_sel >= s_view_count) { s_state = ST_LIST; return; }
        store_pkg *pkg = &s_model.items[s_view[s_sel]];

        if (in->up || in->down)
        {
            s_detail_action = !s_detail_action;
            app_play_sound(a, "navigate");
        }
        if (in->confirm)
        {
            if (s_detail_action == 1) { s_state = ST_LIST; app_play_sound(a, "back"); }
            else if (!pkg->compatible)
            {
                toast_push_error(app_toasts(a), locale_get("incompatible"));
            }
            else if (pkg->installed && !pkg->update_available)
            {
                toast_push(app_toasts(a), locale_get("already_installed"));
            }
            else
            {
                s_state = ST_INSTALLING;   /* paint one frame, then work */
            }
        }
        return;
    }

    if (s_state == ST_INSTALLING)
    {
        if (s_sel < s_view_count)
            start_install(a, &s_model.items[s_view[s_sel]]);
        else
            s_state = ST_LIST;
    }
}

static void ss_draw(app *a, float dt_ms)
{
    discadia_theme *t = app_theme(a);
    layout *L = app_layout(a);
    widget_ctx w = app_widget_ctx(a);

    renderer_draw_background(app_renderer(a), t);

    if (s_state == ST_LOADING || s_state == ST_INSTALLING)
    {
        top_bar_draw(a, locale_get("store"));
        widget_text_primary(&w, FACE_BODY, t->sizes.description_size,
                            locale_get(s_state == ST_INSTALLING ? "installing"
                                                                : "loading"),
                            L->sw / 2, L->icons_cy, 0, 0, 1);
    }
    else if (s_state == ST_LIST)
    {
        char title[128];
        int ups = count_updates();
        if (ups > 0)
            str_format(title, sizeof(title), "%s (%d)",
                       locale_get("store"), ups);
        else
            str_safe_copy(title, locale_get("store"), sizeof(title));
        top_bar_draw(a, title);

        /* Simple vertical list keeps memory flat on small screens. */
        int y0 = L->top_bar.y + L->top_bar.h + 16;
        int wp = L->content_right - L->content_left;
        int lh = t->sizes.title_size + 18;
        int visible = (L->sh - y0 - 70) / lh;
        if (visible < 3) visible = 3;

        char val[64];
        widget_list_row(&w, L->content_left, y0, wp, lh - 6,
                        locale_get("store_search"),
                        s_filter[0] ? s_filter : locale_get("search_placeholder"),
                        -1);
        for (int i = s_scroll; i < s_view_count; i++)
        {
            if (i - s_scroll + 1 >= visible) break;
            store_pkg *p = &s_model.items[s_view[i]];
            val[0] = '\0';
            if (p->update_available) str_safe_copy(val, locale_get("detail_update"), sizeof(val));
            else if (p->installed)   str_safe_copy(val, locale_get("installed"), sizeof(val));
            else if (!p->compatible) str_safe_copy(val, locale_get("incompatible"), sizeof(val));
            widget_list_row(&w, L->content_left,
                            y0 + (i - s_scroll + 1) * lh, wp, lh - 6,
                            p->name, val, i == s_sel);
        }
        widget_text_secondary(&w, FACE_SMALL, t->sizes.meta_size,
                              locale_get("press_back_hint"),
                              L->content_left, L->sh - 44, -1, wp, 1);
    }
    else /* ST_DETAIL */
    {
        top_bar_draw(a, locale_get("store"));
        if (s_sel < s_view_count)
        {
            store_pkg *p = &s_model.items[s_view[s_sel]];
            int x = L->content_left, wp = L->content_right - L->content_left;
            int y = L->top_bar.y + L->top_bar.h + 20;
            widget_text_primary(&w, FACE_TITLE, t->sizes.title_size,
                                p->name, x, y, -1, wp, 1);
            y += t->sizes.title_size + 10;
            char line[128];
            str_format(line, sizeof(line), "%s \xC2\xB7 v%s \xC2\xB7 %d MB",
                       locale_get(pkg_type_locale_key(p->type)),
                       p->version, (int)p->size_mb);
            widget_text_secondary(&w, FACE_SMALL, t->sizes.meta_size,
                                  line, x, y, -1, wp, 1);
            y += t->sizes.meta_size + 16;
            widget_text_primary(&w, FACE_BODY, t->sizes.description_size,
                                p->description, x, y, -1, wp, 6);

            int ly = L->sh - 120;
            const char *act = p->update_available ? locale_get("detail_update")
                                                  : locale_get("detail_install");
            widget_list_row(&w, x, ly, wp, 44, act, "", s_detail_action == 0);
            widget_list_row(&w, x, ly + 52, wp, 44,
                            locale_get("back"), "", s_detail_action == 1);
        }
    }

    toast_draw(app_toasts(a), &w, L->sw);
}

const screen_vtable *screen_store_vtable(void)
{
    static const screen_vtable vt = {
        .name = "store", .enter = ss_enter, .update = ss_update,
        .draw = ss_draw, .leave = ss_leave
    };
    return &vt;
}
