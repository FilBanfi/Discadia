#ifndef DISCADIA_INPUT_HOTPLUG_H
#define DISCADIA_INPUT_HOTPLUG_H

/*
 * Controller hotplug: open every controller present at boot and handle
 * SDL_CONTROLLERDEVICEADDED/REMOVED events pumped from the main loop.
 */

#include <SDL.h>

int  controller_open_index(int joystick_index);
void controller_close_instance(SDL_JoystickID id);
int  controller_count(void);

/* Open all currently-connected game controllers. Returns count opened. */
int input_hotplug_init(void);

/* Handle a device add/remove event. Returns 1 if the event was consumed. */
int input_hotplug_process_event(const SDL_Event *ev);

#endif /* DISCADIA_INPUT_HOTPLUG_H */
