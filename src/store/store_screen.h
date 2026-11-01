#ifndef DISCADIA_STORE_SCREEN_H
#define DISCADIA_STORE_SCREEN_H

/*
 * Store screen vtable (see store_client.h / package_installer.h).
 * States: loading index -> list -> detail -> installing (modal).
 * Includes a "Check for Updates" action that refreshes the index and
 * flags packages whose remote version differs from the local one;
 * applying updates reinstalls them without touching user settings.
 */

#include "app/app.h"

const screen_vtable *screen_store_vtable(void);

#endif /* DISCADIA_STORE_SCREEN_H */
