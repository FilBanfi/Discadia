#include "library/library.h"
#include "library/library_internal.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

/*
 * Library container: dynamically grown array of items. Sorting and
 * lookup helpers live in sorter.c; this file owns memory only.
 */

#define LIB_GROWTH 16

library *library_create(void)
{
    library *lib = calloc(1, sizeof(*lib));
    return lib;
}

void library_destroy(library *lib)
{
    if (!lib) return;
    free(lib->items);
    free(lib);
}

void library_clear(library *lib)
{
    if (!lib) return;
    lib->count = 0;
}

int library_count(const library *lib)
{
    return lib ? lib->count : 0;
}

const library_item *library_get(const library *lib, int index)
{
    if (!lib || index < 0 || index >= lib->count) return NULL;
    return &lib->items[index];
}

library_item *library_get_mut(library *lib, int index)
{
    if (!lib || index < 0 || index >= lib->count) return NULL;
    return &lib->items[index];
}

static int ensure_capacity(library *lib, int need)
{
    if (need <= lib->capacity) return 0;
    int newcap = lib->capacity + LIB_GROWTH * (1 + need / LIB_GROWTH);
    library_item *p = realloc(lib->items, (size_t)newcap * sizeof(*p));
    if (!p) return -1;
    lib->items = p;
    lib->capacity = newcap;
    return 0;
}

int library_add(library *lib, const library_item *item)
{
    if (!lib || !item) return -1;

    /* Replace existing entry with same id+source (disc re-scans). */
    for (int i = 0; i < lib->count; i++)
    {
        library_item *cur = &lib->items[i];
        if (cur->source == item->source && strcmp(cur->id, item->id) == 0 &&
            strcmp(cur->metadata_dir, item->metadata_dir) == 0)
        {
            *cur = *item;
            return i;
        }
    }

    if (ensure_capacity(lib, lib->count + 1) != 0) return -1;
    lib->items[lib->count] = *item;
    return lib->count++;
}

int library_remove_by_dir_prefix(library *lib, const char *prefix)
{
    if (!lib || !prefix) return 0;
    size_t plen = strlen(prefix);
    int removed = 0;
    int w = 0;
    for (int r = 0; r < lib->count; r++)
    {
        if (strncmp(lib->items[r].metadata_dir, prefix, plen) == 0)
        {
            removed++;
            continue;
        }
        if (w != r) lib->items[w] = lib->items[r];
        w++;
    }
    lib->count = w;
    return removed;
}
