#include "library/library.h"
#include "i18n/locale.h"
#include "log.h"

#include "util/json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * Multilingual metadata resolution. For any text field KEY the lookup
 * order is: KEY_<lang>  ->  KEY_en  ->  KEY. This lets a disc ship one
 * default language plus optional translations without breaking when the
 * user's language is missing from the media.
 */

static void copy_field(char *dst, size_t dst_size, const char *src)
{
    snprintf(dst, dst_size, "%s", src ? src : "");
}

/* Resolve string field "base" with locale suffixes into dst:
 * base_<lang> -> base_en -> base. */
static void metadata_get_localized(const discadia_json *root, const char *base,
                                   char *dst, size_t dst_size)
{
    const char *lang = locale_get_current();
    char key[160];

    /* Try KEY_<current_lang> first. */
    if (lang && *lang)
    {
        snprintf(key, sizeof(key), "%s_%s", base, lang);
        discadia_json *n = json_get(root, key);
        const char *v = json_string(n);
        if (v && *v)
        {
            snprintf(dst, dst_size, "%s", v);
            json_free(n);
            return;
        }
        json_free(n);
    }
    /* Then explicit English, then the bare field. */
    snprintf(key, sizeof(key), "%s_en", base);
    discadia_json *n = json_get(root, key);
    const char *v = json_string(n);
    if (v && *v)
    {
        snprintf(dst, dst_size, "%s", v);
        json_free(n);
        return;
    }
    json_free(n);

    n = json_get(root, base);
    v = json_string(n);
    if (v && *v)
    {
        snprintf(dst, dst_size, "%s", v);
        json_free(n);
        return;
    }
    json_free(n);
    dst[0] = '\0';
}

int library_item_load_metadata(library_item *item, const char *dir,
                               item_source_t source, int drive_index)
{
    if (!item || !dir) return -1;
    memset(item, 0, sizeof(*item));
    item->drive_index = drive_index;
    item->source = source;
    snprintf(item->metadata_dir, sizeof(item->metadata_dir), "%s", dir);

    char path[640];
    snprintf(path, sizeof(path), "%s/metadata.json", dir);
    discadia_json *j = json_parse_file(path);
    if (!j)
    {
        LOG_INFO("no usable metadata.json at %s", path);
        return -1;
    }

    copy_field(item->id, sizeof(item->id),
               json_get_string_or(j, "id", ""));
    if (item->id[0] == '\0')
    {
        /* Fall back to directory name as id. */
        const char *slash = strrchr(dir, '/');
        copy_field(item->id, sizeof(item->id), slash ? slash + 1 : dir);
    }

    metadata_get_localized(j, "title", item->title, sizeof(item->title));
    metadata_get_localized(j, "description", item->description,
                           sizeof(item->description));

    copy_field(item->platform, sizeof(item->platform),
               json_get_string_or(j, "platform", ""));
    copy_field(item->publisher, sizeof(item->publisher),
               json_get_string_or(j, "publisher", ""));
    copy_field(item->developer, sizeof(item->developer),
               json_get_string_or(j, "developer", ""));
    copy_field(item->genre, sizeof(item->genre),
               json_get_string_or(j, "genre", ""));
    copy_field(item->version, sizeof(item->version),
               json_get_string_or(j, "version", ""));
    copy_field(item->region, sizeof(item->region),
               json_get_string_or(j, "region", ""));
    item->year = json_get_int_or(j, "year", 0);
    item->players = json_get_int_or(j, "players", 0);

    copy_field(item->engine, sizeof(item->engine),
               json_get_string_or(j, "engine", "native"));
    copy_field(item->game_file, sizeof(item->game_file),
               json_get_string_or(j, "game_file", ""));
    metadata_get_localized(j, "thumbnail", item->thumbnail,
                           sizeof(item->thumbnail));
    if (item->thumbnail[0] == '\0')
        copy_field(item->thumbnail, sizeof(item->thumbnail), "thumbnail.jpg");

    /* Absolute game path: game_file relative to metadata dir. Native
     * apps may instead point "executable" at an absolute binary. */
    const char *exe = json_get_string_or(j, "executable", NULL);
    if (exe && exe[0] == '/')
        snprintf(item->game_path, sizeof(item->game_path), "%s", exe);
    else if (exe && *exe)
        snprintf(item->game_path, sizeof(item->game_path), "%s/%s", dir, exe);
    else if (item->game_file[0])
        snprintf(item->game_path, sizeof(item->game_path), "%s/%s",
                 dir, item->game_file);
    else
        item->game_path[0] = '\0';

    if (item->title[0] == '\0')
        copy_field(item->title, sizeof(item->title), item->id);

    json_free(j);
    return 0;
}

void library_item_meta_line(const library_item *it, char *dst, size_t dst_size,
                            const int show[8], const char *separator)
{
    /* show[] order matches theme.metadata_display:
     * platform, year, publisher, developer, genre, players, version, region */
    static const char *sep_default = " | ";
    const char *sep = (separator && *separator) ? separator : sep_default;
    dst[0] = '\0';

    char tmp[256];
    const char *vals[8];
    snprintf(tmp, sizeof(tmp), "%d", it->year);
    char year_buf[32]; snprintf(year_buf, sizeof(year_buf), "%d", it->year);
    char players_buf[32];
    snprintf(players_buf, sizeof(players_buf), "%d", it->players);

    vals[0] = it->platform;   vals[1] = year_buf;    vals[2] = it->publisher;
    vals[3] = it->developer;  vals[4] = it->genre;   vals[5] = players_buf;
    vals[6] = it->version;    vals[7] = it->region;

    for (int i = 0; i < 8; i++)
    {
        if (!show[i] || !vals[i] || !vals[i][0]) continue;
        if (i == 1 && it->year <= 0) continue;
        if (i == 5 && it->players <= 0) continue;
        if (dst[0] != '\0') strncat(dst, sep, dst_size - strlen(dst) - 1);
        strncat(dst, vals[i], dst_size - strlen(dst) - 1);
    }
    (void)tmp;
}
