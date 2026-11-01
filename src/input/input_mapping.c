#include "input_mapping.h"
#include "util/json.h"
#include "util/fs.h"
#include "log.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Mapping persistence. File format (~/.config/discadia/input-map.json):
 *   { "version": 1, "mappings": { "left": "DPAD_LEFT", ... } }
 * Keyboard fallback bindings are always registered in addition to the
 * stored gamepad mapping, so a broken/empty file never bricks input.
 */

static const char *g_action_names[IA_COUNT] = {
    "left", "right", "up", "down", "confirm", "back", "menu", "context"
};

static const char *g_action_keys[IA_COUNT] = {
    "action_left", "action_right", "action_up", "action_down",
    "action_confirm", "action_back", "action_menu", "action_context"
};

/* Default gamepad bindings (NES-style layout). */
static const char *g_default_bindings[IA_COUNT] = {
    "DPAD_LEFT", "DPAD_RIGHT", "DPAD_UP", "DPAD_DOWN",
    "A", "B", "START", "SELECT"
};

/* Fixed keyboard fallback (never persisted, always active). */
static const char *g_keyboard_fallback[IA_COUNT] = {
    "KEY_LEFT", "KEY_RIGHT", "KEY_UP", "KEY_DOWN",
    "KEY_RETURN", "KEY_ESCAPE", "", ""
};

static char g_binding[IA_COUNT][BTN_NAME_LEN];      /* gamepad binding   */
static char g_kb_binding[IA_COUNT][BTN_NAME_LEN];   /* keyboard fallback */

/* Names recognized by input_event_to_button_name + SELECT + extra keys. */
static int is_known_button(const char *name)
{
    static const char *known[] = {
        "A", "B", "X", "Y", "BACK", "GUIDE", "START", "SELECT",
        "LEFTSTICK", "RIGHTSTICK", "LEFTSHOULDER", "RIGHTSHOULDER",
        "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT",
        "KEY_LEFT", "KEY_RIGHT", "KEY_UP", "KEY_DOWN", "KEY_RETURN",
        "KEY_ESCAPE", "KEY_Q", "KEY_SPACE"
    };
    for (size_t i = 0; i < sizeof(known) / sizeof(known[0]); i++)
        if (strcmp(known[i], name) == 0) return 1;
    return 0;
}

int input_mapping_action_index(const char *action_name)
{
    if (!action_name) return -1;
    for (int a = 0; a < IA_COUNT; a++)
        if (strcmp(g_action_names[a], action_name) == 0) return a;
    return -1;
}

const char *input_mapping_action_key(input_action_t action)
{
    if (action < 0 || action >= IA_COUNT) return "";
    return g_action_keys[action];
}

/* Keyboard fallback lookup used by the input core (declared extern there). */
const char *input_mapping_get_keyboard(input_action_t action);

static void set_binding(char *dst, const char *src)
{
    snprintf(dst, BTN_NAME_LEN, "%s", src ? src : "");
}

int input_mapping_load(void)
{
    char path[512];
    fs_get_config_dir(path, sizeof(path));
    strncat(path, "/input-map.json", sizeof(path) - strlen(path) - 1);

    discadia_json *j = json_parse_file(path);
    if (!j)
    {
        LOG_INFO("no input map at %s, using defaults", path);
        for (int a = 0; a < IA_COUNT; a++)
        {
            set_binding(g_binding[a], g_default_bindings[a]);
            set_binding(g_kb_binding[a], g_keyboard_fallback[a]);
        }
        return 0;
    }

    discadia_json *m = json_get(j, "mappings");
    for (int a = 0; a < IA_COUNT; a++)
    {
        const char *v = NULL;
        if (m)
        {
            discadia_json *item = json_get(m, g_action_names[a]);
            v = json_string(item);   /* borrowed from the tree */
            json_free(item);         /* frees only the wrapper */
        }
        if (v && is_known_button(v))
            set_binding(g_binding[a], v);
        else
            set_binding(g_binding[a], g_default_bindings[a]);
        set_binding(g_kb_binding[a], g_keyboard_fallback[a]);
    }
    json_free(j);
    return 0;
}

int input_mapping_save(void)
{
    discadia_json *root = json_new_object();
    if (!root) return -1;
    json_add_number(root, "version", 1);
    discadia_json *m = json_add_object(root, "mappings");
    if (!m) { json_free(root); return -1; }
    for (int a = 0; a < IA_COUNT; a++)
        json_add_string(m, g_action_names[a], g_binding[a]);

    char *text = json_to_string(root);
    json_free(root);
    if (!text) return -1;

    char path[512];
    fs_get_config_dir(path, sizeof(path));
    strncat(path, "/input-map.json", sizeof(path) - strlen(path) - 1);

    int rc = fs_write_file_atomic(path, text, strlen(text));
    free(text);
    if (rc != 0) LOG_ERROR("failed to save input map to %s", path);
    return rc;
}

const char *input_mapping_get(input_action_t action)
{
    if (action < 0 || action >= IA_COUNT) return "";
    return g_binding[action];
}

/* Second binding lookup used by the input core (keyboard fallback). */
const char *input_mapping_get_keyboard(input_action_t action)
{
    if (action < 0 || action >= IA_COUNT) return "";
    return g_kb_binding[action];
}

int input_mapping_set(input_action_t action, const char *button_name)
{
    if (action < 0 || action >= IA_COUNT) return -1;
    if (!button_name || !is_known_button(button_name)) return -1;
    set_binding(g_binding[action], button_name);

    /* Unbind any other action that shared this button (avoid conflicts). */
    for (int a = 0; a < IA_COUNT; a++)
    {
        if (a != (int)action && strcmp(g_binding[a], button_name) == 0)
            set_binding(g_binding[a], "");
    }
    return 0;
}
