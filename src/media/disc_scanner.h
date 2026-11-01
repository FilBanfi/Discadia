#ifndef DISCADIA_DISC_SCANNER_H
#define DISCADIA_DISC_SCANNER_H

/*
 * Disc scanner: reads metadata.json from a mounted disc/USB root and
 * produces a library item (source = SRC_DISC). A mount without a valid
 * metadata.json is "unrecognized media": the caller shows a toast and
 * ignores it forever after (tracked by the manager).
 */

#include "library/library_item.h"

/* Try to load <mount_path>/metadata.json into item. Returns 0 ok,
 * -1 missing/bad file. drive_index stored for multi-drive support. */
int disc_scan_mount(const char *mount_path, int drive_index,
                    library_item *out_item);

#endif /* DISCADIA_DISC_SCANNER_H */
