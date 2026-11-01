#include "library/library_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/*
 * Sorting rules (from the spec):
 *   1. disc/USB games first, alphabetical by title
 *   2. installed games/apps second, alphabetical by title
 * No visual divider is drawn between the two sections.
 */

static int title_cmp(const library_item *a, const library_item *b)
{
    int r = strcasecmp(a->title, b->title);
    if (r != 0) return r;
    return strcmp(a->id, b->id);
}

static int item_cmp(const void *va, const void *vb)
{
    const library_item *a = va, *b = vb;
    /* SRC_DISC (1) sorts before SRC_INSTALLED (0). */
    if (a->source != b->source) return (b->source > a->source) ? 1 : -1;
    return title_cmp(a, b);
}

void library_sort(library *lib, int *selected_inout)
{
    if (!lib || lib->count <= 0) return;

    /* Track which item identity was selected so we can follow it across
     * the re-sort (qsort is not stable and indices move). */
    char sel_id[128] = "";
    char sel_dir[512] = "";
    if (selected_inout && *selected_inout >= 0 && *selected_inout < lib->count)
    {
        snprintf(sel_id, sizeof(sel_id), "%s", lib->items[*selected_inout].id);
        snprintf(sel_dir, sizeof(sel_dir), "%s",
                 lib->items[*selected_inout].metadata_dir);
    }

    qsort(lib->items, (size_t)lib->count, sizeof(lib->items[0]), item_cmp);

    if (selected_inout && sel_id[0])
    {
        for (int i = 0; i < lib->count; i++)
        {
            if (strcmp(lib->items[i].id, sel_id) == 0 &&
                strcmp(lib->items[i].metadata_dir, sel_dir) == 0)
            {
                *selected_inout = i;
                return;
            }
        }
        *selected_inout = 0; /* item vanished: fall back to first entry */
    }
}
