/*
 * metadata_panel.c - bottom panel under the carousel: wrapped description
 * plus a single "platform • year • ..." line built by library_item_meta_line
 * using the theme's metadata_display flags. No field names or separators
 * are hardcoded here; everything comes from theme + item.
 */

#include "ui/metadata_panel.h"
#include "ui/widgets.h"
#include "library/library_item.h"

#include <string.h>

void metadata_panel_draw(app *a, const library_item *item)
{
    if (!item) return;
    layout *L = app_layout(a);
    discadia_theme *t = app_theme(a);
    widget_ctx w = app_widget_ctx(a);

    int maxw = L->content_right - L->content_left;
    int y = L->description_y;

    /* Description block, wrapped to content width (up to 3 lines). */
    if (item->description[0])
        y += widget_text_secondary(&w, FACE_BODY, t->sizes.description_size,
                                   item->description, L->content_left, y, -1,
                                   maxw, 3);

    /* Metadata row per theme flags. */
    const int show[8] = {
        t->metadata_display.show_platform,
        t->metadata_display.show_year,
        t->metadata_display.show_publisher,
        t->metadata_display.show_developer,
        t->metadata_display.show_genre,
        t->metadata_display.show_players,
        t->metadata_display.show_version,
        t->metadata_display.show_region
    };
    char line[512];
    library_item_meta_line(item, line, sizeof(line), show,
                           t->metadata_display.separator);
    if (line[0])
        widget_text(&w, FACE_SMALL, t->sizes.meta_size,
                    theme_color(t, TCOL_TEXT_SECONDARY),
                    line, L->content_left, L->meta_y, -1, maxw, 2);
}
