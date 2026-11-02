/*
 * menu_screen.c - Start-button overlay menu.
 *
 * Entries: Settings / Store / System info / Reboot / Shutdown / Quit.
 * Destructive entries (Reboot/Shutdown/Quit) open a Yes/No confirmation
 * modal, skipped when settings.confirm_shutdown is off for power items.
 * The menu is a real screen so the carousel position is preserved on
 * return to Home (selection lives in the app context).
 */

#include "ui/menu_screen.h"
#include "ui/widgets.h"
#include "ui/renderer.h"
#include "i18n/locale.h"
#include "input/input.h"
#include "log.h"

#include <string.h>

typedef enum menu_entry {
    ME_SETTINGS = 0,
    ME_STORE,
    ME_SYSTEM_INFO,
    ME_REBOOT,
    ME_SHUTDOWN,
    ME_QUIT,
    ME_COUNT
} menu_entry_t;

static const char *ME_LABEL_KEY[ME_COUNT] = {
    "settings", "store", "system_info", "reboot", "shutdown", "quit"
};
static const char *ME_CONFIRM_KEY[ME_COUNT] = {
    NULL, NULL, NULL, "confirm_reboot", "confirm_shutdown", "confirm_quit"
};

/* Screen-local state (persists across visits; reset on enter). */
static int s_sel = 0;
static int s_confirming = 0;   /* modal open            */
static int s_yes = 1;          /* Yes selected in modal */
static int s_confirm_entry = -1;
static int s_show_sysinfo = 0;

static void menu_enter(app *a)
{
    (void)a;
    s_sel = 0;
    s_confirming = 0;
    s_yes = 1;
    s_show_sysinfo = 0;
}

static void menu_leave(app *a) { (void)a; }

static void open_confirmation(app *a, int entry)
{
    settings_model *s = app_settings(a);
    /* When confirmations are disabled, power actions run directly. */
    if (s && !s->confirm_shutdown &&
        (entry == ME_REBOOT || entry == ME_SHUTDOWN || entry == ME_QUIT))
    {
        app_request_nav(a, entry == ME_REBOOT   ? NAV_REBOOT :
                         entry == ME_SHUTDOWN ? NAV_SHUTDOWN : NAV_QUIT);
        return;
    }
    s_confirming = 1;
    s_yes = 1;
    s_confirm_entry = entry;
    app_play_sound(a, "select");
}

static void confirm_choice(app *a)
{
    int entry = s_confirm_entry;
    s_confirming = 0;
    if (!s_yes) { app_play_sound(a, "back"); return; }
    switch (entry)
    {
        case ME_REBOOT:   app_request_nav(a, NAV_REBOOT);   break;
        case ME_SHUTDOWN: app_request_nav(a, NAV_SHUTDOWN); break;
        case ME_QUIT:     app_request_nav(a, NAV_QUIT);     break;
        default: break;
    }
}

static void menu_update(app *a, float dt_ms, const frame_input *in,
                        nav_request_t *out_nav)
{
    (void)dt_ms;

    if (s_show_sysinfo)
    {
        if (in->back || in->confirm) s_show_sysinfo = 0;
        return;
    }

    if (s_confirming)
    {
        if (in->left || in->right) { s_yes = !s_yes; app_play_sound(a, "navigate"); }
        if (in->up || in->down)    { s_yes = !s_yes; app_play_sound(a, "navigate"); }
        if (in->confirm)           confirm_choice(a);
        else if (in->back)         { s_confirming = 0; app_play_sound(a, "back"); }
        return;
    }

    if (in->up)
    {
        s_sel = (s_sel + ME_COUNT - 1) % ME_COUNT;
        app_play_sound(a, "navigate");
    }
    else if (in->down)
    {
        s_sel = (s_sel + 1) % ME_COUNT;
        app_play_sound(a, "navigate");
    }

    if (in->back || in->menu)
    {
        *out_nav = NAV_BACK;      /* menu -> home (navigation layer) */
        return;
    }

    if (in->confirm)
    {
        switch (s_sel)
        {
            case ME_SETTINGS: *out_nav = NAV_OPEN_SETTINGS; break;
            case ME_STORE:    *out_nav = NAV_OPEN_STORE;    break;
            case ME_SYSTEM_INFO: s_show_sysinfo = 1; break;
            default: open_confirmation(a, s_sel); return;
        }
        app_play_sound(a, "select");
    }
}

static void draw_sysinfo(app *a, widget_ctx *w, layout *L)
{
    discadia_theme *t = app_theme(a);
    char line[256];
    int y = L->sh / 4;
    int cx = L->sw / 2;

    SDL_Color hd = theme_color(t, TCOL_ACCENT);
    widget_text(w, FACE_TITLE, t->sizes.title_size * 3 / 4, hd,
                locale_get("system_info"), cx, y - 70, 0, 0, 1);

    str_format(line, sizeof(line), "%s: %s", locale_get("sys_model"),
               sysinfo_model());
    widget_text_secondary(w, FACE_BODY, t->sizes.description_size, line,
                          cx, y, 0, L->sw - 80, 1);
    str_format(line, sizeof(line), "%s: %s", locale_get("sys_arch"),
               sysinfo_arch());
    widget_text_secondary(w, FACE_BODY, t->sizes.description_size, line,
                          cx, y + 34, 0, L->sw - 80, 1);
    str_format(line, sizeof(line), "%s: %ld MB", locale_get("sys_ram"),
               sysinfo_total_ram_mb());
    widget_text_secondary(w, FACE_BODY, t->sizes.description_size, line,
                          cx, y + 68, 0, L->sw - 80, 1);
    str_format(line, sizeof(line), "%s: v%s", locale_get("version_label"),
               app_version());
    widget_text_secondary(w, FACE_BODY, t->sizes.description_size, line,
                          cx, y + 102, 0, L->sw - 80, 1);

    widget_text_secondary(w, FACE_SMALL, t->sizes.meta_size,
                          locale_get("press_back_hint"), cx, y + 170, 0,
                          0, 1);
}

static void menu_draw(app *a, float dt_ms)
{
    (void)dt_ms;
    discadia_theme *t = app_theme(a);
    layout *L = app_layout(a);
    widget_ctx w = app_widget_ctx(a);

    /* Dim the home screen behind: translucent full-screen veil. */
    renderer_fill_rounded(app_renderer(a), 0, 0, L->sw, L->sh, 0,
                          theme_color(t, TCOL_BACKGROUND), 190);

    if (s_show_sysinfo) { draw_sysinfo(a, &w, L); return; }

    int row_h = t->sizes.title_size + 26;
    int box_w = L->sw * 4 / 10; if (box_w < 320) box_w = 320;
    int box_h = row_h * ME_COUNT + 60;
    int bx = (L->sw - box_w) / 2;
    int by = (L->sh - box_h) / 2;

    renderer_fill_rounded(app_renderer(a), bx, by, box_w, box_h,
                          t->sizes.icon_border_radius * 2,
                          theme_color(t, TCOL_SECONDARY), 235);
    renderer_outline_rounded(app_renderer(a), bx, by, box_w, box_h,
                             t->sizes.icon_border_radius * 2,
                             theme_color(t, TCOL_ACCENT), 2);

    widget_text_primary(&w, FACE_TITLE, t->sizes.title_size * 3 / 4,
                        locale_get("menu"), L->sw / 2, by + 16, 0, 0, 1);

    for (int i = 0; i < ME_COUNT; i++)
    {
        widget_list_row(&w, bx + 14, by + 56 + i * row_h,
                        box_w - 28, row_h - 8,
                        locale_get(ME_LABEL_KEY[i]), NULL, i == s_sel);
    }

    if (s_confirming && s_confirm_entry >= 0 &&
        s_confirm_entry < ME_COUNT && ME_CONFIRM_KEY[s_confirm_entry])
    {
        widget_confirm_box(&w, L->sw, L->sh,
                           locale_get(ME_CONFIRM_KEY[s_confirm_entry]),
                           s_yes);
    }
}

const screen_vtable *screen_menu_vtable(void)
{
    static const screen_vtable vt = {
        .name = "menu", .enter = menu_enter, .update = menu_update,
        .draw = menu_draw, .leave = menu_leave
    };
    return &vt;
}
