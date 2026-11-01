#ifndef DISCADIA_LIBRARY_H
#define DISCADIA_LIBRARY_H

/*
 * Game library: an ordered array of items (disc games first, then
 * installed games, each alphabetical by title). The carousel reads this
 * directly; media events call library_add_disc()/library_remove_disc()
 * and the UI updates in place.
 */

#include "library/library_item.h"

typedef struct library library;

library *library_create(void);
void     library_destroy(library *lib);

/* Remove every item (used before a full rescan). */
void library_clear(library *lib);

int  library_count(const library *lib);           /* 0 if NULL          */
const library_item *library_get(const library *lib, int index); /* NULL oob */

/* Insert/replace an item (takes a copy). Returns its index or -1. */
int library_add(library *lib, const library_item *item);

/* Remove all items whose metadata_dir starts with prefix (a mountpoint).
 * Returns number removed. Keeps list sorted. */
int library_remove_by_dir_prefix(library *lib, const char *prefix);

/* Re-sort: disc games first (alphabetical), then installed (alphabetical).
 * If selected_in was valid it is remapped to the same item's new index
 * through out_selected (may be NULL). */
void library_sort(library *lib, int *selected_inout);

#endif /* DISCADIA_LIBRARY_H */
