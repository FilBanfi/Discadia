#ifndef DISCADIA_CAROUSEL_INTERNAL_H
#define DISCADIA_CAROUSEL_INTERNAL_H
/* Internal: accessors implemented in app.c for carousel animation state. */
#include "app/app.h"
float app_carousel_anim_pos(app *a);
void  app_set_carousel_anim_pos(app *a, float pos);
int   app_carousel_target(const app *a);
#endif
