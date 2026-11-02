/*
 * store_index.c - parse the router repository's index.json into the
 * in-memory store model. Multilingual fields use the same resolution
 * rule as game metadata: key_<lang> -> key_en -> key (never fails).
 */

#include <stdlib.h>
#include <string.h>
#include "store/store_index.h"
#include "util/json.h"
#include "util/fs.h"
#include "i18n/locale.h"
#include "log.h"

/* Localized string pick, mirroring library/metadata.c rules. */
static void idx_get_localized(const discadia_json *obj, const char *base,
                              char *dst, size_t dst_size)
{
    const char *lang = locale_get_current();
    char key[80];
    dst[0] = '\0';

    if (lang && *lang && strcmp(lang, "en") != 0)
    {
        snprintf(key, sizeof(key), "%s_%s", base, lang);
        const char *v = json_get_string_or(obj, key, NULL);
        if (v && *v) { snprintf(dst, dst_size, "%s", v); return; }
    }
    snprintf(key, sizeof(key), "%s_en", base);
    const char *v = json_get_string_or(obj, key, NULL);
    if (v && *v) { snprintf(dst, dst_size, "%s", v); return; }
    v = json_get_string_or(obj, base, NULL);
    if (v && *v) snprintf(dst, dst_size, "%s", v);
}

static int parse_one(const discadia_json *item, store_pkg *out)
{
    memset(out, 0, sizeof(*out));
    const char *id = json_get_string_or(item, "id", NULL);
    if (!id || !*id) return -1;
    snprintf(out->id, sizeof(out->id), "%s", id);

    idx_get_localized(item, "name", out->name, sizeof(out->name));
    if (!out->name[0]) snprintf(out->name, sizeof(out->name), "%s", id);

    idx_get_localized(item, "description", out->description,
                      sizeof(out->description));

    const char *type = json_get_string_or(item, "type", "game");
    out->type = pkg_type_from_string(type);

    const char *ver = json_get_string_or(item, "version", "");
    snprintf(out->version, sizeof(out->version), "%s", ver);

    out->size_mb = json_get_int_or(item, "size_mb", 0);

    const char *mu = json_get_string_or(item, "manifest_url", NULL);
    if (mu) snprintf(out->manifest_url, sizeof(out->manifest_url), "%s", mu);
    else snprintf(out->manifest_url, sizeof(out->manifest_url),
                  "packages/%s/manifest.json", id);

    const char *cu = json_get_string_or(item, "cover_url", "");
    snprintf(out->cover_url, sizeof(out->cover_url), "%s", cu);

    discadia_json *archs = json_get(item, "compatible_hardware");
    int n = json_array_size(archs);
    for (int i = 0; i < n && out->arch_count < 8; i++)
    {
        const char *a = json_string(json_array_get(archs, i));
        if (a) snprintf(out->archs[out->arch_count++], 16, "%s", a);
    }
    return 0;
}

int store_index_parse(const char *json_text, store_model *out)
{
    if (!json_text || !out) return -1;
    store_model_clear(out);

    discadia_json *root = json_parse(json_text);
    if (!root)
    {
        LOG_W("store: index JSON malformed");
        return -1;
    }
    const char *v = json_get_string_or(root, "version", "");
    snprintf(out->index_version, sizeof(out->index_version), "%s", v);
    const char *lu = json_get_string_or(root, "last_updated", "");
    snprintf(out->last_updated, sizeof(out->last_updated), "%s", lu);

    discadia_json *pkgs = json_get(root, "packages");
    int n = json_array_size(pkgs);
    for (int i = 0; i < n && out->count < ST_MAX_PACKAGES; i++)
    {
        if (parse_one(json_array_get(pkgs, i), &out->items[out->count]) == 0)
            out->count++;
        else
            LOG_W("store: skipping package entry %d (missing id)", i);
    }
    json_free(root);
    LOG_I("store: index parsed, %d packages", out->count);
    return 0;
}

int store_index_parse_file(const char *path, store_model *out)
{
    char *text = fs_read_file(path, NULL);
    if (!text) return -1;
    int rc = store_index_parse(text, out);
    free(text);
    return rc;
}
