#include "locale.h"
#include "util/json.h"
#include "util/path_util.h"
#include "util/string_util.h"
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ENTRIES 256
#define MAX_KEY_LEN 128
#define MAX_VALUE_LEN 512

typedef struct {
    char key[MAX_KEY_LEN];
    char value[MAX_VALUE_LEN];
} LocaleEntry;

typedef struct {
    char code[16];
    LocaleEntry entries[MAX_ENTRIES];
    int count;
} LocaleData;

static LocaleData current_locale;
static int initialized = 0;

static int load_locale_file(const char *lang_code) {
    char path[512];
    
    // Try data/locales/{code}.json first (dev mode)
    path_join(path, sizeof(path), "data/locales", lang_code);
    str_safe_cat(path, ".json", sizeof(path));
    
    size_t size = 0;
    char *data = NULL;
    
    // Read file manually to access cJSON directly
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        // Try installed location
        path_join(path, sizeof(path), "/etc/discadia/locales", lang_code);
        str_safe_cat(path, ".json", sizeof(path));
        f = fopen(path, "rb");
        if (f == NULL) {
            fprintf(stderr, "locale: cannot open %s\n", lang_code);
            return -1;
        }
    }
    
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    data = malloc(size + 1);
    if (data == NULL) {
        fclose(f);
        return -1;
    }
    
    fread(data, 1, size, f);
    data[size] = '\0';
    fclose(f);
    
    cJSON *root = cJSON_Parse(data);
    free(data);
    
    if (root == NULL) {
        fprintf(stderr, "locale: failed to parse JSON for %s\n", lang_code);
        return -1;
    }
    
    // Clear old entries
    current_locale.count = 0;
    str_safe_copy(current_locale.code, lang_code, sizeof(current_locale.code));
    
    // Iterate all key-value pairs
    cJSON *child = NULL;
    cJSON_ArrayForEach(child, root) {
        if (current_locale.count >= MAX_ENTRIES) break;
        if (!cJSON_IsString(child)) continue;
        
        LocaleEntry *entry = &current_locale.entries[current_locale.count];
        str_safe_copy(entry->key, child->string, MAX_KEY_LEN);
        str_safe_copy(entry->value, child->valuestring, MAX_VALUE_LEN);
        current_locale.count++;
    }
    
    cJSON_Delete(root);
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
