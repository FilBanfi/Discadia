/*
 * disc_scanner.c - see disc_scanner.h
 */

#include "media/disc_scanner.h"
#include "util/fs.h"
#include "util/string_util.h"
#include "log.h"

#include <string.h>

int disc_scan_mount(const char *mount_path, int drive_index,
                    library_item *out_item)
{
    if (!mount_path || !out_item) return -1;
    char meta[640];
    str_format(meta, sizeof(meta), "%s/metadata.json", mount_path);
    if (!fs_file_exists(meta)) return -1;

    if (library_item_load_metadata(out_item, mount_path, SRC_DISC,
                                   drive_index) != 0)
        return -1;
    LOG_INFO("disc scanned at %s: '%s'", mount_path, out_item->title);
    return 0;
}
