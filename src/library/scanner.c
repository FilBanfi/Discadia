#include "library/scanner.h"
#include "util/fs.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * One-level directory walk of the installed games root. Each subfolder
 * with a metadata.json becomes an item. Broken metadata is skipped (a
 * bad game folder must never break the whole library).
 */

typedef struct {
    library *lib;
    char root[512];
    int added;
} scan_ctx;

static int dir_entry_cb(const char *name, const char *full_path,
                        int is_dir, void *userdata)
{
    scan_ctx *ctx = userdata;
    if (!is_dir || name[0] == '.') return 0; /* skip hidden files/dirs */

    library_item item;
    if (library_item_load_metadata(&item, full_path, SRC_INSTALLED, -1) == 0)
    {
        if (library_add(ctx->lib, &item) >= 0) ctx->added++;
    }
    return 0;
}

int scanner_scan_installed(library *lib)
{
    if (!lib) return -1;

    char root[512];
    fs_get_data_dir(root, sizeof(root));
    if (!fs_dir_exists(root))
    {
        LOG_INFO("games root %s does not exist yet", root);
        return 0;
    }

    /* Rebuild the installed section from scratch: collect every
     * installed item's metadata_dir first (removing while iterating
     * would shift indices), then drop them one by one. Disc entries
     * are always preserved. */
    char (*dirs)[512] = NULL;
    int ndirs = 0;
    int n = library_count(lib);
    if (n > 0)
    {
        dirs = malloc((size_t)n * sizeof(*dirs));
        if (!dirs) return -1;
        for (int i = 0; i < n; i++)
        {
            const library_item *it = library_get(lib, i);
            if (it && it->source == SRC_INSTALLED)
                snprintf(dirs[ndirs++], 512, "%s", it->metadata_dir);
        }
        for (int i = 0; i < ndirs; i++)
            library_remove_by_dir_prefix(lib, dirs[i]);
        free(dirs);
    }

    scan_ctx ctx = { lib, "", 0 };
    snprintf(ctx.root, sizeof(ctx.root), "%s", root);
    if (fs_list_dir(root, dir_entry_cb, &ctx) != 0)
    {
        LOG_WARN("failed to list %s", root);
        return -1;
    }
    LOG_INFO("scanner: %d installed game(s) found in %s", ctx.added, root);
    return ctx.added;
}
