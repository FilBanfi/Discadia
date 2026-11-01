/*
 * library_item.c - helpers around the plain library_item struct.
 * Metadata parsing lives in metadata.c; this TU provides shared small
 * utilities used by scanner/sorter/screens so behavior stays uniform.
 */

#include "library/library_item.h"
#include "util/string_util.h"

#include <string.h>
#include <stdio.h>

/* Build a stable id from folder name when metadata.json omits one:
 * lowercase, non-alphanumerics collapsed to '-'. */
void library_item_default_id(library_item *item, const char *dir)
{
    if (!item || item->id[0]) return;
    const char *base = strrchr(dir, '/');
    base = base ? base + 1 : dir;
    size_t j = 0;
    for (const char *p = base; *p && j < sizeof(item->id) - 1; p++)
    {
        char c = *p;
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) item->id[j++] = c;
        else if (c >= 'A' && c <= 'Z') item->id[j++] = (char)(c - 'A' + 'a');
        else if (j > 0 && item->id[j-1] != '-') item->id[j++] = '-';
    }
    while (j > 0 && item->id[j-1] == '-') j--;
    item->id[j] = '\0';
}
