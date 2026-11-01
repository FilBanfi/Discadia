#ifndef DISCADIA_INPUT_H
#define DISCADIA_INPUT_H

/*
 * Input abstraction: screens never look at SDL keycodes or gamepad buttons.
 * They poll logical actions (LEFT/RIGHT/UP/DOWN/CONFIRM/BACK/MENU/CONTEXT)
 * filled once per frame by input_poll(). Mapping is user-remappable and
 * persisted in ~/.config/discadia/input-map.json.
 */

#include <SDL.h>

typedef enum input_action {
    IA_LEFT = 0,
    IA_RIGHT,
    IA_UP,
    IA_DOWN,
    IA_CONFIRM,
    IA_BACK,
    IA_MENU,
    IA_CONTEXT,
    IA_COUNT
} input_action_t;

/* Per-frame state of one action. */
typedef struct input_state {
    int down;      /* currently held                 */
    int pressed;   /* went down this frame (edge)    */
    int released;  /* went up this frame (edge)      */
    float hold_ms; /* how long it has been held      */
} input_state;

int  input_init(void);
void input_shutdown(void);

/* Pump SDL events and refresh action states. Call exactly once per frame. */
void input_poll(void);

/* Query an action. Returns zeroed state if out of range. */
input_state input_get(input_action_t action);

/* Any confirm/back press this frame? Convenience for modal waits. */
int input_any_pressed(void);

/* Human-readable name of the physical button currently bound to an action
 * (e.g. "A", "DPAD_LEFT"). Static buffer per action. */
const char *input_action_binding_name(input_action_t action);

/* Translate a raw SDL event into the physical button name used by the
 * mapping files ("A", "B", "START", "DPAD_LEFT", "KEY_RETURN", ...).
 * Returns NULL when the event is not mappable. Used by the remap screen. */
const char *input_event_to_button_name(const SDL_Event *ev);

#endif /* DISCADIA_INPUT_H */
