/*
 * store_index.c - parses the store repository's index.json into a
 * store_model. Expected shape (see discadia-store repo):
 *
 *   { "version": "12", "last_updated": "...",
 *     "packages": [ { "id","name","type","version","size_mb",
 *                     "description" | "description_<lang>",
 *                     "compatible_hardware": ["arm64", ...],
 *                     "manifest": "packages/<id>/manifest.json",
 *                     "cover":    "packages/<id>/cover.png" } ] }
 *
 * Multilingual text follows the metadata rule: description_<current>
 * first, then description_en, then the bare key.
 */

#include "store/store_index.h"
#include "util/json.h"
#include "util/fs.h"
#include "util/string_util.h"
#include "i18n/locale.h"
#include "log.h"

#include <string.h>
#include <stdlib.h> /* free() on parsed strings */

/* Copy the best available localized string for `base` ("description"). */
static void pick_localized(discadia_json *obj, const char *base,
                           char *dst, size_t dstsz)
{
    char key[64];
    str_format(key, sizeof(key), "%s_%s", base, locale_get_current());
    const char *v = json_get_string_or(obj, key, NULL);
    if (!v)
    {
        str_format(key, sizeof(key), "%s_en", base);
        v = json_get_string_or(obj, key, NULL);
    }
    if (!v) v = json_get_string_or(obj, base, "");
    str_safe_copy(dst, v, dstsz);
}

int store_index_parse(const char *json_text, store_model *out)
{
    if (!json_text || !out) return -1;
    store_model_clear(out);

    discadia_json *root = json_parse(json_text);
    if (!root) { LOG_WARN("store: index JSON parse failed"); return -1; }

    str_safe_copy(out->index_version,
                  json_get_string_or(root, "version", "0"),
                  sizeof(out->index_version));
    str_safe_copy(out->last_updated,
                  json_get_string_or(root, "last_updated", ""),
                  sizeof(out->last_updated));

    discadia_json *pkgs = json_get(root, "packages");
    int n = json_array_size(pkgs);
    for (int i = 0; i < n && out->count < ST_MAX_PACKAGES; i++)
    {
        discadia_json *pj = json_array_get(pkgs, i);
        if (!pj) continue;
        store_pkg *p = &out->items[out->count];

        str_safe_copy(p->id,   json_get_string_or(pj, "id", ""),   sizeof(p->id));
        if (!p->id[0]) continue;                       /* skip invalid entry */
        str_safe_copy(p->name, json_get_string_or(pj, "name", p->id),
                      sizeof(p->name));
        pick_localized(pj, "description", p->description, sizeof(p->description));
        p->type = pkg_type_from_string(json_get_string_or(pj, "type", "game"));
        str_safe_copy(p->version, json_get_string_or(pj, "version", "0"),
                      sizeof(p->version));
        p->size_mb = json_get_int_or(pj, "size_mb", 0);
        str_safe_copy(p->manifest_url, json_get_string_or(pj, "manifest", ""),
                      sizeof(p->manifest_url));
        str_safe_copy(p->cover_url, json_get_string_or(pj, "cover", ""),
                      sizeof(p->cover_url));

        discadia_json *archs = json_get(pj, "compatible_hardware");
        int ac = json_array_size(archs);
        for (int k = 0; k < ac && p->arch_count < 8; k++)
        {
            const char *a = json_string(json_array_get(archs, k));
            if (a && *a)
                str_safe_copy(p->archs[p->arch_count++], a,
                              sizeof(p->archs[0]));
        }
        out->count++;
    }

    json_free(root);
    LOG_INFO("store: index parsed, %d packages (v%s)",
          out->count, out->index_version);
    return 0;
}

int store_index_parse_file(const char *path, store_model *out)
{
    char *text = fs_read_file(path, NULL);
    if (!text) { LOG_WARN("store: cannot read index %s", path); return -1; }
    int rc = store_index_parse(text, out);
    free(text);
    return rc;
}
