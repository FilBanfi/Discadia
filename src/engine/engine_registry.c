/*
 * engine_registry.c - see engine_registry.h
 */

#include "engine/engine_registry.h"
#include "util/json.h"
#include "util/fs.h"
#include "util/string_util.h"
#include "util/path_util.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>

#define REGISTRY_MAX_ENGINES 64

struct engine_registry {
    engine items[REGISTRY_MAX_ENGINES];
    int    count;
};

engine_registry *engine_registry_create(void)
{
    return calloc(1, sizeof(engine_registry));
}

void engine_registry_destroy(engine_registry *r) { free(r); }

int engine_registry_count(const engine_registry *r) { return r ? r->count : 0; }

static int parse_one_engine(engine *out, const char *path)
{
    discadia_json *j = json_parse_file(path);
    if (!j)
    {
        LOG_WARN("engine: cannot parse %s", path);
        return -1;
    }
    memset(out, 0, sizeof(*out));
    str_safe_copy(out->id, json_get_string_or(j, "id", ""), sizeof(out->id));
    if (out->id[0] == '\0')
    {
        /* Fall back to filename without extension. */
        char base[128];
        path_basename(base, sizeof(base), path);
        char *dot = strrchr(base, '.');
        if (dot) *dot = '\0';
        str_safe_copy(out->id, base, sizeof(out->id));
    }
    str_safe_copy(out->name, json_get_string_or(j, "name", out->id),
                  sizeof(out->name));
    str_safe_copy(out->command, json_get_string_or(j, "command", ""),
                  sizeof(out->command));
    str_safe_copy(out->save_dir_template,
                  json_get_string_or(j, "save_dir_template",
                                     "{saves_base}/{game_id}"),
                  sizeof(out->save_dir_template));
    str_safe_copy(out->platform, json_get_string_or(j, "platform", ""),
                  sizeof(out->platform));

    discadia_json *exts = json_get(j, "supported_extensions");
    if (exts)
    {
        int n = json_array_size(exts);
        for (int i = 0; i < n && out->extension_count < ENGINE_MAX_EXT; i++)
        {
            const char *e = json_string(json_array_get(exts, i));
            if (e && *e)
                str_safe_copy(out->extensions[out->extension_count++],
                              e, sizeof(out->extensions[0]));
        }
        json_free(exts);
    }
    json_free(j);
    if (out->command[0] == '\0')
    {
        LOG_WARN("engine %s has empty command, ignored", out->id);
        return -1;
    }
    return 0;
}

static int collect_cb(const char *name, const char *full_path, int is_dir,
                      void *userdata)
{
    if (is_dir) return 0;
    engine_registry *r = userdata;
    if (!str_ends_with(name, ".json")) return 0;
    if (r->count >= REGISTRY_MAX_ENGINES) return 0;

    /* Skip duplicates by id. */
    engine tmp;
    if (parse_one_engine(&tmp, full_path) != 0) return 0;
    for (int i = 0; i < r->count; i++)
    {
        if (strcmp(r->items[i].id, tmp.id) == 0)
        {
            LOG_WARN("engine id '%s' duplicated (%s), keeping first",
                     tmp.id, name);
            return 0;
        }
    }
    r->items[r->count++] = tmp;
    LOG_INFO("engine loaded: %s (%s)", tmp.id, tmp.name);
    return 0;
}

int engine_registry_load_dir(engine_registry *r, const char *dir)
{
    if (!r || !dir) return -1;
    if (!fs_dir_exists(dir))
    {
        LOG_WARN("engines dir missing: %s", dir);
        return 0;   /* not fatal: user may have no engines yet */
    }
    int before = r->count;
    fs_list_dir(dir, collect_cb, r);
    return r->count - before;
}

const engine *engine_registry_find(const engine_registry *r, const char *id)
{
    if (!r || !id || !*id) return NULL;
    for (int i = 0; i < r->count; i++)
        if (strcmp(r->items[i].id, id) == 0) return &r->items[i];
    return NULL;
}
