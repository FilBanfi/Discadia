#ifndef DISCADIA_BOOT_SCREEN_H
#define DISCADIA_BOOT_SCREEN_H
/* Procedurally drawn boot splash (no external assets required):
 * gradient background, glowing wordmark, spinning disc arc, then a
 * "press any button" prompt. Replaces the host system splash. */
#include "app/app.h"
const screen_vtable *screen_boot_vtable(void);
#endif
