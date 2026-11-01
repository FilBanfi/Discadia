#include "json.h"
#include "fs.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

struct discadia_json {
    cJSON *root;
    int owns; // 1 if we should free root, 0 if borrowed
};

discadia_json *json_parse(const char *text) {
    if (text == NULL) return NULL;
    cJSON *root = cJSON_Parse(text);
    if (root == NULL) return NULL;
    
    discadia_json *j = malloc(sizeof(*j));
    if (j == NULL) {
        cJSON_Delete(root);
        return NULL;
    }
    j->root = root;
    j->owns = 1;
    return j;
}

discadia_json *json_parse_file(const char *path) {
    size_t size = 0;
    char *data = fs_read_file(path, &size);
    if (data == NULL) return NULL;
    
    discadia_json *j = json_parse(data);
    free(data);
    return j;
}

void json_free(discadia_json *j) {
    if (j == NULL) return;
    if (j->owns && j->root != NULL) {
        cJSON_Delete(j->root);
    }
    free(j);
}

discadia_json *json_get(const discadia_json *j, const char *key) {
    if (j == NULL || j->root == NULL || key == NULL) return NULL;
    cJSON *child = cJSON_GetObjectItemCaseSensitive(j->root, key);
    if (child == NULL) return NULL;
    
    discadia_json *wrap = malloc(sizeof(*wrap));
    if (wrap == NULL) return NULL;
    wrap->root = child;
    wrap->owns = 0; // borrowed, don't free
    return wrap;
}

discadia_json *json_array_get(const discadia_json *j, int index) {
    if (j == NULL || j->root == NULL) return NULL;
    cJSON *child = cJSON_GetArrayItem(j->root, index);
    if (child == NULL) return NULL;
    
    discadia_json *wrap = malloc(sizeof(*wrap));
    if (wrap == NULL) return NULL;
    wrap->root = child;
    wrap->owns = 0;
    return wrap;
}

int json_array_size(const discadia_json *j) {
    if (j == NULL || j->root == NULL) return 0;
    return cJSON_GetArraySize(j->root);
}

const char *json_string(const discadia_json *j) {
    if (j == NULL || j->root == NULL) return NULL;
    return cJSON_GetStringValue(j->root);
}

const char *json_string_or(const discadia_json *j, const char *def) {
    const char *s = json_string(j);
    return s ? s : def;
}

int json_int_or(const discadia_json *j, int def) {
    if (j == NULL || j->root == NULL) return def;
    if (cJSON_IsNumber(j->root)) return j->root->valueint;
    return def;
}

double json_double_or(const discadia_json *j, double def) {
    if (j == NULL || j->root == NULL) return def;
    if (cJSON_IsNumber(j->root)) return j->root->valuedouble;
    return def;
}

int json_bool_or(const discadia_json *j, int def) {
    if (j == NULL || j->root == NULL) return def;
    if (cJSON_IsBool(j->root)) return cJSON_IsTrue(j->root) ? 1 : 0;
    return def;
}

int json_exists(const discadia_json *j) {
    return j != NULL && j->root != NULL && !cJSON_IsNull(j->root);
}

const char *json_get_string_or(const discadia_json *j, const char *key, const char *def) {
    if (j == NULL || j->root == NULL || key == NULL) return def;
    cJSON *child = cJSON_GetObjectItemCaseSensitive(j->root, key);
    if (child == NULL) return def;
    const char *s = cJSON_GetStringValue(child);
    return s ? s : def;
}

int json_get_int_or(const discadia_json *j, const char *key, int def) {
    if (j == NULL || j->root == NULL || key == NULL) return def;
    cJSON *child = cJSON_GetObjectItemCaseSensitive(j->root, key);
    if (child == NULL || !cJSON_IsNumber(child)) return def;
    return child->valueint;
}

int json_get_bool_or(const discadia_json *j, const char *key, int def) {
    if (j == NULL || j->root == NULL || key == NULL) return def;
    cJSON *child = cJSON_GetObjectItemCaseSensitive(j->root, key);
    if (child == NULL || !cJSON_IsBool(child)) return def;
    return cJSON_IsTrue(child) ? 1 : 0;
}

char *json_to_string(const discadia_json *j) {
    if (j == NULL || j->root == NULL) return NULL;
    char *raw = cJSON_PrintUnformatted(j->root);
    return raw; // caller frees with cJSON-free... we need to wrap this
}
