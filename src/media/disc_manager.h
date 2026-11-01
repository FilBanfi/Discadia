#ifndef DISCADIA_DISC_MANAGER_H
#define DISCADIA_DISC_MANAGER_H

/*
 * Disc manager: watches the mount root (/media/discadia by default,
 * overridable with DISCADIA_MEDIA_DIR for testing) once per second with
 * a non-blocking timer - no threads, no udev hard dependency. New
 * mounts containing metadata.json become disc library items; vanished
 * mounts are removed from the library. Mounts without valid metadata
 * are reported once as "unrecognized media" and then ignored until they
 * disappear (prevents toast spam from data/audio CDs).
 */

#include <SDL.h>
#include "library/library.h"

typedef struct disc_manager disc_manager;

disc_manager *disc_manager_create(const char *media_root);
void          disc_manager_destroy(disc_manager *m);

/* Poll the mount dir at most once per second. Returns bitmask of
 * events since last call: bit0 = a disc was inserted, bit1 = a disc
 * was removed, bit2 = an unrecognized media appeared. When bits are
 * set, `library` has already been updated in place. */
int disc_manager_update(disc_manager *m, library *lib, Uint32 now_ms);

/* Number of currently tracked drives with a mounted disc. */
int disc_manager_active_count(const disc_manager *m);

#endif /* DISCADIA_DISC_MANAGER_H */
