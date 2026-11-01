#ifndef DISCADIA_HOME_SCREEN_H
#define DISCADIA_HOME_SCREEN_H
/* Main library screen: top bar + carousel + metadata panel + empty state. */
#include "app/app.h"
const screen_vtable *screen_home_vtable(void);
/* Shared carousel drawing helpers (also used by menu/store lists). */
void home_draw_top_bar(app *a, int title_source_item); /* -1 = app title */
#endif
