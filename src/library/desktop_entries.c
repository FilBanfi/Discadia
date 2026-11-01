/*
 * desktop_entries.c - OPTIONAL XDG .desktop import (disabled by default).
 *
 * Discadia's primary sources are physical media and store-installed
 * folders. On desktop systems users sometimes keep games as freedesktop
 * entries; when DISCADIA_IMPORT_DESKTOP=1 is set the app can import
 * them as installed items using the "native" engine. Off by default so
 * the Pi target never pays for it.
 */

#include "library/library.h"
#include "library/library_item.h"
#include "util/fs.h"
#include "util/string_util.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* tiny local join to avoid pulling path_util into a hot include chain */
static void path_join_local(char *dst, size_t size, const char *a, const char *b)
{
    snprintf(dst, size, "%s/%s", a, b);
}

typedef struct { char key[64], val[256]; } kv;

typedef struct { library *lib; int added; const char *dir; } de_ctx;

static int parse_desktop_file(const char *path, kv *out, int max)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[512];
    int n = 0, in_entry = 0;
    while (fgets(line, sizeof(line), f))
    {
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '[')
        {
            in_entry = (strncmp(s, "[Desktop Entry]", 15) == 0);
            continue;
        }
        if (!in_entry || s[0] == '#' || s[0] == '\n') continue;
        char *eq = strchr(s, '=');
        if (!eq || n >= max) continue;
        size_t kl = (size_t)(eq - s);
        if (kl >= sizeof(out[n].key)) continue;
        memcpy(out[n].key, s, kl); out[n].key[kl] = '\0';
        char *v = eq + 1;
        size_t vl = strcspn(v, "\r\n");
        if (vl >= sizeof(out[n].val)) vl = sizeof(out[n].val) - 1;
        memcpy(out[n].val, v, vl); out[n].val[vl] = '\0';
        n++;
    }
    fclose(f);
    return n;
}

static const char *kv_get(kv *arr, int n, const char *key)
{
    for (int i = 0; i < n; i++)
        if (!strcmp(arr[i].key, key)) return arr[i].val;
    return NULL;
}

static int de_cb(const char *name, const char *full, int is_dir, void *ud)
{
    (void)name;
    de_ctx *c = ud;
    if (is_dir || !str_ends_with(full, ".desktop")) return 0;

    kv fields[24];
    int n = parse_desktop_file(full, fields, 24);
    const char *exec = kv_get(fields, n, "Exec");
    const char *nm   = kv_get(fields, n, "Name");
    const char *no   = kv_get(fields, n, "NoDisplay");
    const char *hid  = kv_get(fields, n, "Hidden");
    if (!exec || (no && !strcmp(no, "true")) || (hid && !strcmp(hid, "true")))
        return 0;

    library_item it;
    memset(&it, 0, sizeof(it));
    str_safe_copy(it.id, nm ? nm : exec, sizeof(it.id));
    str_to_lower(it.id);
    str_safe_copy(it.title, nm ? nm : it.id, sizeof(it.title));
    str_safe_copy(it.engine, "native", sizeof(it.engine));
    /* Native engine: the Exec line IS the command (game_path used raw
     * by variables.c for {game_path}). */
    str_safe_copy(it.game_path, exec, sizeof(it.game_path));
    str_safe_copy(it.metadata_dir, c->dir, sizeof(it.metadata_dir));
    it.source = SRC_INSTALLED;
    it.drive_index = -1;
    if (library_add(c->lib, &it) >= 0) c->added++;
    return 0;
}

int desktop_entries_import(library *lib)
{
    if (!lib) return -1;
    static char buf0[768], buf1[768];
    const char *dirs[3];
    int nd = 0;
    const char *xdg = getenv("XDG_DATA_HOME");
    if (xdg && *xdg)
    {
        path_join_local(buf0, sizeof(buf0), xdg, "applications");
        dirs[nd++] = buf0;
    }
    else
    {
        const char *home = getenv("HOME");
        path_join_local(buf0, sizeof(buf0), home ? home : "/root",
                        ".local/share/applications");
        dirs[nd++] = buf0;
    }
    (void)buf1;
    dirs[nd++] = "/usr/share/applications";

    int total = 0;
    for (int d = 0; d < nd; d++)
    {
        if (!fs_dir_exists(dirs[d])) continue;
        de_ctx c = { lib, 0, dirs[d] };
        fs_list_dir(dirs[d], de_cb, &c);
        total += c.added;
    }
    LOG_INFO("desktop entries imported: %d", total);
    return total;
}
