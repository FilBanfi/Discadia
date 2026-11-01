#ifndef DISCADIA_METADATA_PANEL_H
#define DISCADIA_METADATA_PANEL_H
#include "app/app.h"
#include "library/library_item.h"
/* Description + "platform • year • ..." line for the selected item. */
void metadata_panel_draw(app *a, const library_item *item);
#endif
