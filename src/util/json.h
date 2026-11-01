#ifndef DISCADIA_JSON_H
#define DISCADIA_JSON_H

// Thin wrapper around cJSON to isolate third-party dependency.
// All JSON operations in Discadia go through these functions.

#include <stddef.h>

// Opaque handle
typedef struct discadia_json discadia_json;

// Parse a JSON string. Returns NULL on error. Caller must free with json_free.
discadia_json *json_parse(const char *text);

// Parse a JSON file. Returns NULL on error. Caller must free with json_free.
discadia_json *json_parse_file(const char *path);

// Free a JSON tree.
void json_free(discadia_json *j);

// Get a child object by key. Returns NULL if not found. Not owned.
discadia_json *json_get(const discadia_json *j, const char *key);

// Get a child by array index. Returns NULL if out of bounds. Not owned.
discadia_json *json_array_get(const discadia_json *j, int index);

// Get array length. Returns 0 if not an array.
int json_array_size(const discadia_json *j);

// Get string value. Returns NULL if not a string or missing.
const char *json_string(const discadia_json *j);

// Get string value with default.
const char *json_string_or(const discadia_json *j, const char *def);

// Get integer value. Returns def if not a number or missing.
int json_int_or(const discadia_json *j, int def);

// Get float value. Returns def if not a number or missing.
double json_double_or(const discadia_json *j, double def);

// Get boolean value. Returns def if not a bool or missing.
int json_bool_or(const discadia_json *j, int def);

// Check if value exists and is not null.
int json_exists(const discadia_json *j);

// Convenience: get child string with default
const char *json_get_string_or(const discadia_json *j, const char *key, const char *def);

// Convenience: get child int with default
int json_get_int_or(const discadia_json *j, const char *key, int def);

// Convenience: get child bool with default
int json_get_bool_or(const discadia_json *j, const char *key, int def);

// Serialize to string. Caller must free. Returns NULL on error.
char *json_to_string(const discadia_json *j);

/* ---- Builder API (thin wrappers over cJSON creation functions).
 * The returned object is owned by the caller: free with json_free().
 * Children added via these helpers are owned by the parent. */

discadia_json *json_new_object(void);
discadia_json *json_new_array(void);

/* Add a child value created from primitives; returns the added child
 * (borrowed pointer, owned by parent) or NULL on error. */
discadia_json *json_add_string(discadia_json *parent, const char *key, const char *value);
discadia_json *json_add_number(discadia_json *parent, const char *key, double value);
discadia_json *json_add_bool(discadia_json *parent, const char *key, int value);
discadia_json *json_add_object(discadia_json *parent, const char *key);
void           json_array_append(discadia_json *array, discadia_json *item);

/* Iterate object children: pass prev=NULL to start. Returns borrowed
 * child wrapped in a fresh non-owning handle (free wrapper with json_free),
 * or NULL at the end. Key name via json_child_key(). */
discadia_json *json_iter(const discadia_json *parent, const discadia_json *prev);
const char *json_child_key(const discadia_json *child);

#endif
