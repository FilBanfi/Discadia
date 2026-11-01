/*
 * disc_manager.c - see disc_manager.h
 */

#include "media/disc_manager.h"
#include "media/disc_scanner.h"
#include "util/fs.h"
#include "util/string_util.h"
#include "log.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#define DM_MAX_DRIVES 8

typedef struct drive_slot {
    char mount_path[512];   /* empty when slot free                  */
    int  unrecognized;      /* mounted but no metadata -> ignore     */
} drive_slot;

struct disc_manager {
    char media_root[512];
    drive_slot slots[DM_MAX_DRIVES];
    Uint32 last_poll_ms;
};

disc_manager *disc_manager_create(const char *media_root)
{
    disc_manager *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    str_safe_copy(m->media_root,
                  media_root && *media_root ? media_root : "/media/discadia",
                  sizeof(m->media_root));
    LOG_INFO("media: watching %s", m->media_root);
    return m;
}

void disc_manager_destroy(disc_manager *m) { free(m); }

int disc_manager_active_count(const disc_manager *m)
{
    if (!m) return 0;
    int n = 0;
    for (int i = 0; i < DM_MAX_DRIVES; i++)
        if (m->slots[i].mount_path[0] && !m->slots[i].unrecognized) n++;
    return n;
}

static int find_mount_index(disc_manager *m, const char *path)
{
    for (int i = 0; i < DM_MAX_DRIVES; i++)
        if (strcmp(m->slots[i].mount_path, path) == 0) return i;
    return -1;
}

static int claim_free_slot(disc_manager *m, const char *path)
{
    for (int i = 0; i < DM_MAX_DRIVES; i++)
    {
        if (!m->slots[i].mount_path[0])
        {
            str_safe_copy(m->slots[i].mount_path, path,
                          sizeof(m->slots[i].mount_path));
            m->slots[i].unrecognized = 0;
            return i;
        }
    }
    return -1;
}

int disc_manager_update(disc_manager *m, library *lib, Uint32 now_ms)
{
    if (!m || !lib) return 0;
    if (now_ms - m->last_poll_ms < 1000) return 0;   /* 1 Hz polling */
    m->last_poll_ms = now_ms;

    int events = 0;

    /* Snapshot current mount dirs under media_root. */
    char seen[DM_MAX_DRIVES][512];
    int  seen_n = 0;
    DIR *d = opendir(m->media_root);
    if (d)
    {
        struct dirent *e;
        while ((e = readdir(d)) != NULL)
        {
            if (e->d_name[0] == '.') continue;
            char full[640];
            str_format(full, sizeof(full), "%s/%s", m->media_root, e->d_name);
            if (!fs_dir_exists(full)) continue;
            if (seen_n < DM_MAX_DRIVES)
                str_safe_copy(seen[seen_n++], full, sizeof(seen[0]));
        }
        closedir(d);
    }

    /* Removals: tracked slot whose path is gone. */
    for (int i = 0; i < DM_MAX_DRIVES; i++)
    {
        if (!m->slots[i].mount_path[0]) continue;
        int still = 0;
        for (int s = 0; s < seen_n; s++)
            if (strcmp(seen[s], m->slots[i].mount_path) == 0) { still = 1; break; }
        if (still) continue;

        if (!m->slots[i].unrecognized)
        {
            library_remove_by_dir_prefix(lib, m->slots[i].mount_path);
            events |= 2;
            LOG_INFO("media removed: %s", m->slots[i].mount_path);
        }
        m->slots[i].mount_path[0] = '\0';
    }

    /* Additions: visible mounts we do not track yet. */
    for (int s = 0; s < seen_n; s++)
    {
        if (find_mount_index(m, seen[s]) >= 0) continue;
        int idx = claim_free_slot(m, seen[s]);
        if (idx < 0) break;   /* too many mounts; retry next poll */

        library_item item;
        if (disc_scan_mount(seen[s], idx, &item) == 0)
        {
            library_add(lib, &item);
            library_sort(lib, NULL);
            events |= 1;
            LOG_INFO("media inserted: %s ('%s')", seen[s], item.title);
        }
        else
        {
            m->slots[idx].unrecognized = 1;
            events |= 4;
            LOG_INFO("media at %s has no usable metadata.json", seen[s]);
        }
    }

    return events;
}
