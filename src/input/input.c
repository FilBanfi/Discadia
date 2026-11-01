#include "input/input.h"
#include "input/input_mapping.h"
#include "input/input_hotplug.h"
#include "log.h"

#include <SDL.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*
 * Input core: pumps SDL events once per frame, translates physical
 * buttons (gamepad via user mapping, keyboard via fixed fallback) into
 * the eight logical actions and tracks press/release edges + hold time.
 * A tiny ring buffer carries hotplug notifications to the app layer so
 * input_poll() stays event-driven and never blocks.
 */

#define REPEAT_DELAY_MS 380.0f
#define REPEAT_RATE_MS  160.0f

/* Fixed keys (macros live in input.h so the remap screen shares them). */

static struct {
    int down;
    float hold_ms;
    int pressed;
    int released;
} g_actions[IA_COUNT];

/* Quit edge from the fixed Q key (works even if BACK is remapped). */
static int g_quit_pressed;

/* Hotplug notification queue consumed by the app layer each frame. */
#define HOTPLUG_QUEUE 8
static int g_hp_events[HOTPLUG_QUEUE]; /* 1 = connected, -1 = disconnected */
static int g_hp_head, g_hp_count;

extern const char *input_mapping_get_keyboard(input_action_t action);

/* Set when SDL_QUIT arrives; consumed by input_window_close_requested(). */
static int g_app_quit_requested;

int input_init(void)
{
    memset(g_actions, 0, sizeof(g_actions));
    g_quit_pressed = 0;
    g_hp_head = g_hp_count = 0;
    input_mapping_load();
    return 0;
}

void input_shutdown(void)
{
    /* Mapping persistence is done by the remap screen right after each
     * change; nothing else to release here (controllers are closed by
     * SDL_QuitSubSystem in app shutdown). */
}

static void hp_push(int dir)
{
    int tail = (g_hp_head + g_hp_count) % HOTPLUG_QUEUE;
    if (g_hp_count < HOTPLUG_QUEUE)
    {
        g_hp_events[tail] = dir;
        g_hp_count++;
    }
}

int input_pop_hotplug_event(void)
{
    if (g_hp_count <= 0) return 0;
    int e = g_hp_events[g_hp_head];
    g_hp_head = (g_hp_head + 1) % HOTPLUG_QUEUE;
    g_hp_count--;
    return e;
}

static int action_for_button_name(const char *name)
{
    if (!name || !*name) return -1;
    for (int a = 0; a < IA_COUNT; a++)
    {
        const char *bound = input_mapping_get((input_action_t)a);
        if (bound && *bound && strcmp(bound, name) == 0) return a;
        const char *kb = input_mapping_get_keyboard((input_action_t)a);
        if (kb && *kb && strcmp(kb, name) == 0) return a;
    }
    return -1;
}

static void set_down(int action, int is_down, float dt_ms)
{
    if (action < 0 || action >= IA_COUNT) return;
    if (is_down && !g_actions[action].down)
    {
        g_actions[action].pressed = 1;
        g_actions[action].hold_ms = 0.0f;
    }
    else if (!is_down && g_actions[action].down)
    {
        g_actions[action].released = 1;
    }
    g_actions[action].down = is_down;
    if (is_down) g_actions[action].hold_ms += dt_ms;
}

/* Pump one SDL event into the action states. Shared by input_poll(). */
static void pump_event(const SDL_Event *ev, float dt_ms)
{
    switch (ev->type)
    {
    case SDL_QUIT:
        g_app_quit_requested = 1;
        break;

    case SDL_CONTROLLERDEVICEADDED:
        if (controller_open_index(ev->cdevice.which) == 0) hp_push(1);
        break;
    case SDL_CONTROLLERDEVICEREMOVED:
        controller_close_instance(ev->cdevice.which);
        hp_push(-1);
        break;

    case SDL_KEYDOWN:
    case SDL_KEYUP:
    {
        /* Ignore autorepeat but still track the held state, otherwise a
         * key pressed before a long screen pause could stay "down" in our
         * bookkeeping forever. */
        const int repeat = (ev->type == SDL_KEYDOWN && ev->key.repeat);
        const char *name = input_event_to_button_name(ev);
        int is_down = (ev->type == SDL_KEYDOWN);
        if (name && strcmp(name, FIXED_QUIT_KEY) == 0)
        {
            if (is_down && !repeat) g_quit_pressed = 1;
            break;
        }
        if (name && strcmp(name, FIXED_CONTEXT_KEY) == 0)
        {
            set_down(IA_CONTEXT, is_down, dt_ms);
            break;
        }
        {
            int action = action_for_button_name(name);
            if (action >= 0 && !repeat) set_down(action, is_down, dt_ms);
            else if (action >= 0) g_actions[action].down = is_down; /* sync on repeat */
        }
        break;
    }

    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
    {
        const char *name = input_event_to_button_name(ev);
        if (name)
            set_down(action_for_button_name(name),
                     ev->type == SDL_CONTROLLERBUTTONDOWN, dt_ms);
        break;
    }

    default:
        break;
    }
}

int input_window_close_requested(void)
{
    int q = g_app_quit_requested;
    g_app_quit_requested = 0;
    return q;
}

void input_pump_pending_events(void)
{
    /* Drain events without touching timing (used while waiting on a
     * launched game so hotplug/quit keep working). */
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) pump_event(&ev, 0.0f);
}

void input_poll(void)
{
    static Uint64 perf_freq;
    static Uint64 last_ticks;
    float dt_ms = 0.0f;
    Uint64 now = SDL_GetPerformanceCounter();
    if (perf_freq == 0) perf_freq = SDL_GetPerformanceFrequency();
    if (last_ticks != 0)
        dt_ms = (float)((now - last_ticks) * 1000.0 / perf_freq);
    last_ticks = now;
    if (dt_ms > 250.0f) dt_ms = 250.0f; /* window was hidden etc. */

    for (int a = 0; a < IA_COUNT; a++)
    {
        g_actions[a].pressed = 0;
        g_actions[a].released = 0;
        if (g_actions[a].down) g_actions[a].hold_ms += dt_ms;
    }
    g_quit_pressed = 0;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) pump_event(&ev, dt_ms);
}

input_state input_get(input_action_t action)
{
    input_state s = {0, 0, 0, 0.0f};
    if (action < 0 || action >= IA_COUNT) return s;
    s.down = g_actions[action].down;
    s.pressed = g_actions[action].pressed;
    s.released = g_actions[action].released;
    s.hold_ms = g_actions[action].hold_ms;

    /* Synthetic auto-repeat while held (navigation feel). */
    if (s.down && s.hold_ms > REPEAT_DELAY_MS)
    {
        float phase = fmodf(s.hold_ms - REPEAT_DELAY_MS, REPEAT_RATE_MS);
        if (phase < 16.7f) s.pressed = 1;
    }
    return s;
}

int input_any_pressed(void)
{
    return g_actions[IA_CONFIRM].pressed || g_actions[IA_BACK].pressed;
}

int input_quit_pressed(void)
{
    return g_quit_pressed;
}

const char *input_action_binding_name(input_action_t action)
{
    const char *b = input_mapping_get(action);
    if (b && *b) return b;
    return input_mapping_get_keyboard(action);
}

/* ---- raw event -> physical button name (shared with remap screen) ---- */

static const char *g_controller_buttons[] = {
    "A", "B", "X", "Y", "BACK", "GUIDE", "START",
    "LEFTSTICK", "RIGHTSTICK", "LEFTSHOULDER", "RIGHTSHOULDER",
    "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT"
};

const char *input_event_to_button_name(const SDL_Event *ev)
{
    if (!ev) return NULL;

    if (ev->type == SDL_CONTROLLERBUTTONDOWN ||
        ev->type == SDL_CONTROLLERBUTTONUP)
    {
        /* SDL calls the old "SELECT/BACK" pad button SDL_BUTTON_BACK; we
         * expose it as "SELECT" (classic NES naming used in mappings). */
        if (ev->cbutton.button == SDL_CONTROLLER_BUTTON_BACK) return "SELECT";
        Uint8 b = ev->cbutton.button;
        if (b < sizeof(g_controller_buttons) / sizeof(g_controller_buttons[0]))
            return g_controller_buttons[b];
        return NULL;
    }

    if (ev->type == SDL_KEYDOWN || ev->type == SDL_KEYUP)
    {
        switch (ev->key.keysym.sym)
        {
        case SDLK_LEFT:    return "KEY_LEFT";
        case SDLK_RIGHT:   return "KEY_RIGHT";
        case SDLK_UP:      return "KEY_UP";
        case SDLK_DOWN:    return "KEY_DOWN";
        case SDLK_RETURN:
        case SDLK_KP_ENTER:return "KEY_RETURN";
        case SDLK_ESCAPE:  return "KEY_ESCAPE";
        case SDLK_q:       return "KEY_Q";
        case SDLK_SPACE:   return "KEY_SPACE";
        default:           return NULL;
        }
    }
    return NULL;
}
