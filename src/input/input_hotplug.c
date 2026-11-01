#include "input/input_hotplug.h"
#include "log.h"

#include <SDL.h>

int input_hotplug_init(void)
{
    int opened = 0;
    int n = SDL_NumJoysticks();
    for (int i = 0; i < n; i++)
        if (controller_open_index(i) == 0) opened++;
    LOG_INFO("hotplug init: %d controller(s) opened of %d joystick(s)",
             opened, n);
    return opened;
}

int input_hotplug_process_event(const SDL_Event *ev)
{
    if (!ev) return 0;
    switch (ev->type)
    {
    case SDL_CONTROLLERDEVICEADDED:
        controller_open_index(ev->cdevice.which);
        return 1;
    case SDL_CONTROLLERDEVICEREMOVED:
        controller_close_instance(ev->cdevice.which);
        return 1;
    default:
        return 0;
    }
}
