#include "locale.h"
#include "util/json.h"
#include "util/path_util.h"
#include "util/string_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LOCALES 16
#define MAX_KEY_LEN 128
#define MAX_VALUE_LEN 512

typedef struct {
    char key[MAX_KEY_LEN];
    char value[MAX_VALUE_LEN];
} LocaleEntry;

typedef struct {
    char code[16];
    LocaleEntry entries[MAX_LOCALES * 64];
    int count;
} LocaleData;

static LocaleData current_locale;
static int initialized = 0;

static int load_locale_file(const char *lang_code) {
    char path[512];
    
    // Try data/locales/{code}.json first (dev mode)
    path_join(path, sizeof(path), "data/locales", lang_code);
    str_safe_cat(path, ".json", sizeof(path));
    
    discadia_json *json = json_parse_file(path);
    
    // If not found, try /etc/discadia/locales/{code}.json (installed mode)
    if (json == NULL) {
        path_join(path, sizeof(path), "/etc/discadia/locales", lang_code);
        str_safe_cat(path, ".json", sizeof(path));
        json = json_parse_file(path);
    }
    
    if (json == NULL) {
        fprintf(stderr, "locale: failed to load %s\n", lang_code);
        return -1;
    }
    
    // Clear old entries
    current_locale.count = 0;
    str_safe_copy(current_locale.code, lang_code, sizeof(current_locale.code));
    
    // Parse all key-value pairs (assuming flat JSON object)
    // We'll iterate through the root object
    // Note: this is a simplified approach; for production we'd need cJSON iteration
    
    json_free(json);
    return 0;
}

int locale_init(const char *lang_code) {
    if (lang_code == NULL) lang_code = "en";
    
    if (load_locale_file(lang_code) != 0) {
        // Fallback to English
        if (strcmp(lang_code, "en") != 0) {
            return load_locale_file("en");
        }
        return -1;
    }
    
    initialized = 1;
    return 0;
}

const char *locale_get(const char *key) {
    if (!initialized || key == NULL) return key;
    
    for (int i = 0; i < current_locale.count; i++) {
        if (strcmp(current_locale.entries[i].key, key) == 0) {
            return current_locale.entries[i].value;
        }
    }
    
    return key; // Return key as fallback
}

const char *locale_get_current(void) {
    return current_locale.code;
}

int locale_set(const char *lang_code) {
    if (lang_code == NULL) return -1;
    return load_locale_file(lang_code);
}

void locale_cleanup(void) {
    current_locale.count = 0;
    initialized = 0;
}
