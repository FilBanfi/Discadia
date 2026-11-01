/*
 * locale.c - dynamic i18n backed by a cJSON tree kept in memory.
 * Keys are looked up on demand (no fixed-size table), so locale files can
 * grow without touching the code. Fallback chain: requested lang -> "en".
 */

#include "locale.h"
#include "util/json.h"
#include "util/fs.h"
#include "util/path_util.h"
#include "util/string_util.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static discadia_json *g_root = NULL;      /* parsed current language   */
static discadia_json *g_en   = NULL;      /* parsed English fallback   */
static char g_code[16] = "en";

/* Load one locale file; returns parsed tree or NULL. */
static discadia_json *load_file(const char *data_dir, const char *code) {
    char path[768];
    path_join(path, sizeof(path), data_dir, "locales");
    char full[800];
    str_format(full, sizeof(full), "%s/%s.json", path, code);

    if (!fs_file_exists(full)) return NULL;
    discadia_json *j = json_parse_file(full);
    if (j == NULL) {
        LOG_WARN("locale: malformed JSON in %s", full);
    }
    return j;
}

/* Replace "{name}" occurrences of up to 2 placeholders into dst. */
static void fill_placeholders(char *dst, size_t dst_size, const char *tmpl,
                               const char *n1, const char *v1,
                               const char *n2, const char *v2) {
    if (dst == NULL || dst_size == 0) return;
    dst[0] = '\0';
    if (tmpl == NULL) return;

    size_t pos = 0;
    for (const char *p = tmpl; *p && pos + 1 < dst_size; ) {
        int matched = 0;
        const char *val = NULL;
        size_t tag_len = 0;

        if (n1 != NULL && v1 != NULL && p[0] == '{' &&
            strncmp(p + 1, n1, strlen(n1)) == 0 && p[1 + strlen(n1)] == '}') {
            val = v1; tag_len = strlen(n1) + 2; matched = 1;
        } else if (n2 != NULL && v2 != NULL && p[0] == '{' &&
                   strncmp(p + 1, n2, strlen(n2)) == 0 && p[1 + strlen(n2)] == '}') {
            val = v2; tag_len = strlen(n2) + 2; matched = 1;
        }

        if (matched) {
            size_t vlen = strlen(val);
            if (pos + vlen >= dst_size) vlen = dst_size - pos - 1;
            memcpy(dst + pos, val, vlen);
            pos += vlen;
            p += tag_len;
        } else {
            dst[pos++] = *p++;
        }
    }
    dst[pos] = '\0';
}

static int do_load(const char *data_dir, const char *lang_code) {
    if (data_dir == NULL) return -1;
    if (lang_code == NULL || lang_code[0] == '\0') lang_code = "en";

    discadia_json *req = load_file(data_dir, lang_code);
    if (req == NULL && strcmp(lang_code, "en") != 0) {
        LOG_INFO("locale '%s' unavailable, falling back to 'en'", lang_code);
        req = load_file(data_dir, "en");
        if (req != NULL) str_safe_copy(g_code, "en", sizeof(g_code));
    } else if (req != NULL) {
        str_safe_copy(g_code, lang_code, sizeof(g_code));
    }

    if (req == NULL) return -1;

    /* Keep English around as a second-level fallback for missing keys. */
    if (strcmp(g_code, "en") != 0 && g_en == NULL) {
        g_en = load_file(data_dir, "en");
    }

    json_free(g_root);
    g_root = req;
    return 0;
}

int locale_init(const char *data_dir, const char *lang_code) {
    return do_load(data_dir, lang_code);
}

int locale_set(const char *data_dir, const char *lang_code) {
    return do_load(data_dir, lang_code);
}

const char *locale_get(const char *key) {
    if (key == NULL) return "";
    const char *s = NULL;
    if (g_root != NULL) s = json_get_string_or(g_root, key, NULL);
    if (s == NULL && g_en != NULL) s = json_get_string_or(g_en, key, NULL);
    return s != NULL ? s : key; /* graceful: show the raw key */
}

void locale_fmt(char *out, size_t out_size, const char *key,
                const char *ph_name1, const char *ph_val1,
                const char *ph_name2, const char *ph_val2) {
    fill_placeholders(out, out_size, locale_get(key),
                      ph_name1, ph_val1, ph_name2, ph_val2);
}

const char *locale_get_current(void) {
    return g_code;
}

struct loc_list_ctx {
    locale_list_cb cb;
    void *userdata;
    int count;
};

static int loc_file_cb(const char *name, const char *full_path, int is_dir,
                       void *userdata) {
    struct loc_list_ctx *ctx = (struct loc_list_ctx *)userdata;
    if (is_dir || ctx == NULL || !str_ends_with(name, ".json")) return 0;

    char code[16];
    size_t n = strlen(name) - 5; /* strip .json */
    if (n == 0 || n >= sizeof(code)) return 0;
    memcpy(code, name, n);
    code[n] = '\0';

    /* native_name is an optional "language_name" key inside the file. */
    const char *native = code;
    discadia_json *j = json_parse_file(full_path);
    const char *got = NULL;
    if (j != NULL) got = json_get_string_or(j, "language_name", NULL);
    if (got != NULL) native = got;

    int stop = ctx->cb(code, native, ctx->userdata);
    json_free(j);
    if (stop == 0) ctx->count++;
    return stop;
}

int locale_list_available(const char *data_dir, locale_list_cb cb, void *userdata) {
    if (data_dir == NULL || cb == NULL) return -1;
    char locales_dir[600];
    path_join(locales_dir, sizeof(locales_dir), data_dir, "locales");

    struct loc_list_ctx ctx = { cb, userdata, 0 };
    if (fs_list_dir(locales_dir, loc_file_cb, &ctx) != 0) return -1;
    return ctx.count;
}
