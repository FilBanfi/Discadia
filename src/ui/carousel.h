#ifndef DISCADIA_CAROUSEL_H
#define DISCADIA_CAROUSEL_H
/* Horizontal game carousel with animated scrolling and selection glow.
 * The animated position lives in the app context so it survives between
 * screens (returning from Settings keeps the same spot). */
#include "app/app.h"
void carousel_update(app *a, float dt_ms, const frame_input *in, int *nav_out);
void carousel_draw(app *a, float dt_ms);
/* Adjust logical selection by +/-1 with wraparound + navigate sound. */
void carousel_move(app *a, int delta);
/* Jump selection to first/last item (context button shortcuts). */
void carousel_jump(app *a, int to_last);
#endif
