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

/* Fixed keys handled outside the remappable mapping (defined in input.c,
 * used by the remap screen so it never captures quit/context keys). */
#define FIXED_QUIT_KEY    "KEY_Q"
#define FIXED_CONTEXT_KEY "KEY_SPACE"

/* Hotplug notifications queued by the event pump and consumed by the
 * app layer once per frame. Returns 1 (connected), -1 (disconnected)
 * or 0 when nothing is pending. */
int input_pop_hotplug_event(void);

/* SDL quit event or fixed Q key requested application exit. Clears the
 * flag when read. */
int input_window_close_requested(void);

/* Drain pending SDL events without frame timing (used while a game runs
 * so hotplug and window-close keep working). */
void input_pump_pending_events(void);

/* Translate a raw SDL event into the physical button name used by the
 * mapping files ("A", "B", "START", "DPAD_LEFT", "KEY_RETURN", ...).
 * Returns NULL when the event is not mappable. Used by the remap screen. */
const char *input_event_to_button_name(const SDL_Event *ev);

#endif /* DISCADIA_INPUT_H */
