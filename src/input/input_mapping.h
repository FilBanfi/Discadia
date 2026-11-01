#ifndef DISCADIA_INPUT_MAPPING_H
#define DISCADIA_INPUT_MAPPING_H

/*
 * Logical-action <-> physical-button mapping, persisted in
 * ~/.config/discadia/input-map.json. Keyboard fallback bindings are
 * always active in parallel so the UI is never uncontrollable.
 */

#include "input/input.h"

#define BTN_NAME_LEN 32

/* Load mapping from disk; missing file keeps defaults (returns 0). */
int  input_mapping_load(void);

/* Persist current mapping atomically. Returns 0 on success. */
int  input_mapping_save(void);

/* Which physical button is bound to an action ("A", "DPAD_LEFT", ...). */
const char *input_mapping_get(input_action_t action);

/* Rebind an action to a physical button name. Returns -1 if unknown. */
int  input_mapping_set(input_action_t action, const char *button_name);

/* Action id by name used in the JSON file ("confirm", "back", ...).
 * Returns -1 when unknown. */
int input_mapping_action_index(const char *action_name);

/* Localized-ish label key for an action ("act_left" ...) for the UI. */
const char *input_mapping_action_key(input_action_t action);

#endif /* DISCADIA_INPUT_MAPPING_H */
