/*
 * settings_store.c - persistence of user config via the JSON builder API.
 * Writes are atomic (fs_write_file_atomic) so a power cut can never leave
 * a corrupt config behind; the previous version is kept as config.json.bak.
 */

#include "settings_store.h"
#include "util/fs.h"
#include "util/path_util.h"
#include "util/string_util.h"
#include "util/json.h"
#include "log.h"

#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void settings_store_path(char *dst, size_t dst_size) {
    char dir[512];
    fs_get_config_dir(dir, sizeof(dir));
    path_join(dst, dst_size, dir, "config.json");
}

int settings_store_load(settings_model *m) {
    if (m == NULL) return -1;
    settings_set_defaults(m);

    char path[600];
    settings_store_path(path, sizeof(path));
    if (!fs_file_exists(path)) {
        LOG_INFO("no config file at %s, using defaults", path);
        return 0;
    }

    discadia_json *root = json_parse_file(path);
    if (root == NULL) {
        LOG_WARN("config malformed, using defaults: %s", path);
        return 0;
    }

    const char *theme = json_get_string_or(root, "theme", NULL);
    if (theme != NULL) str_safe_copy(m->theme, theme, sizeof(m->theme));
    const char *lang = json_get_string_or(root, "language", NULL);
    if (lang != NULL) str_safe_copy(m->language, lang, sizeof(m->language));

    discadia_json *audio = json_get(root, "audio");
    if (audio != NULL) {
        m->audio_enabled = json_get_bool_or(audio, "enabled", m->audio_enabled);
        m->audio_volume = (float)json_double_or(json_get(audio, "volume"),
                                                m->audio_volume);
        json_free(audio);
    }

    discadia_json *anim = json_get(root, "animations");
    if (anim != NULL) {
        m->animations_enabled = json_get_bool_or(anim, "enabled", m->animations_enabled);
        json_free(anim);
    }

    discadia_json *sys = json_get(root, "system");
    if (sys != NULL) {
        m->show_clock = json_get_bool_or(sys, "show_clock", m->show_clock);
        const char *fmt = json_get_string_or(sys, "clock_format", NULL);
        if (fmt != NULL) {
            m->clock_format = (strcmp(fmt, "12h") == 0) ? CLOCK_FORMAT_12H
                                                         : CLOCK_FORMAT_24H;
        }
        m->confirm_shutdown = json_get_bool_or(sys, "confirm_shutdown",
                                               m->confirm_shutdown);
        json_free(sys);
    }

    discadia_json *store = json_get(root, "store");
    if (store != NULL) {
        m->store_auto_update_index = json_get_bool_or(store, "auto_update_index",
                                                      m->store_auto_update_index);
        m->store_cache_days = json_get_int_or(store, "cache_days", m->store_cache_days);
        json_free(store);
    }

    json_free(root);
    LOG_DEBUG("config loaded: theme=%s lang=%s", m->theme, m->language);
    return 0;
}

int settings_store_save(const settings_model *m) {
    if (m == NULL) return -1;

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return -1;

    cJSON_AddNumberToObject(root, "version", m->version);
    cJSON_AddStringToObject(root, "theme", m->theme);
    cJSON_AddStringToObject(root, "language", m->language);

    cJSON *audio = cJSON_AddObjectToObject(root, "audio");
    if (audio) {
        cJSON_AddBoolToObject(audio, "enabled", m->audio_enabled);
        cJSON_AddNumberToObject(audio, "volume", m->audio_volume);
    }

    cJSON *anim = cJSON_AddObjectToObject(root, "animations");
    if (anim) cJSON_AddBoolToObject(anim, "enabled", m->animations_enabled);

    cJSON *sys = cJSON_AddObjectToObject(root, "system");
    if (sys) {
        cJSON_AddBoolToObject(sys, "show_clock", m->show_clock);
        cJSON_AddStringToObject(sys, "clock_format",
                                m->clock_format == CLOCK_FORMAT_12H ? "12h" : "24h");
        cJSON_AddBoolToObject(sys, "confirm_shutdown", m->confirm_shutdown);
    }

    cJSON *store = cJSON_AddObjectToObject(root, "store");
    if (store) {
        cJSON_AddBoolToObject(store, "auto_update_index", m->store_auto_update_index);
        cJSON_AddNumberToObject(store, "cache_days", m->store_cache_days);
    }

    char *text = cJSON_Print(root);
    cJSON_Delete(root);
    if (text == NULL) return -1;

    char dir[512];
    fs_get_config_dir(dir, sizeof(dir));
    fs_mkdir_p(dir, 0755);

    char path[600];
    settings_store_path(path, sizeof(path));

    int rc = fs_write_file_atomic(path, text, strlen(text));
    free(text);

    if (rc != 0) LOG_ERROR("failed to save config to %s", path);
    return rc;
}
