#include "input/input.h"
#include "log.h"

#include <SDL.h>

/*
 * Game controller support. SDL's GameController API normalizes every
 * supported pad to the same virtual buttons; the event translation
 * itself lives in input.c (input_event_to_button_name). This file only
 * manages device opening so controllers work even when plugged in after
 * Discadia started (hotplug events are handled here via input_hotplug.c).
 */

static int g_controller_count;

int controller_open_index(int joystick_index)
{
    if (!SDL_IsGameController(joystick_index)) return -1;
    SDL_GameController *c = SDL_GameControllerOpen(joystick_index);
    if (!c)
    {
        LOG_WARN("failed to open controller %d: %s", joystick_index, SDL_GetError());
        return -1;
    }
    g_controller_count++;
    LOG_INFO("controller opened: %s (total %d)",
             SDL_GameControllerName(c), g_controller_count);
    return 0;
}

void controller_close_instance(SDL_JoystickID id)
{
    SDL_GameController *c = SDL_GameControllerFromInstanceID(id);
    if (!c) return;
    SDL_GameControllerClose(c);
    if (g_controller_count > 0) g_controller_count--;
    LOG_INFO("controller disconnected (total %d)", g_controller_count);
}

int controller_count(void)
{
    return g_controller_count;
}
