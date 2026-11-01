#ifndef DISCADIA_LIBRARY_INTERNAL_H
#define DISCADIA_LIBRARY_INTERNAL_H

/*
 * Internal (not public) view of the library container. Only modules that
 * need direct array access (sorter, scanner helpers) include this; UI code
 * uses the public accessors in library.h.
 */

#include "library/library.h"

struct library {
    library_item *items;
    int count;
    int capacity;
};

/* Mutable item access (internal). Returns NULL when out of bounds. */
library_item *library_get_mut(library *lib, int index);

#endif
