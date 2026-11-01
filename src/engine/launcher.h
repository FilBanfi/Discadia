#ifndef DISCADIA_LAUNCHER_H
#define DISCADIA_LAUNCHER_H

/*
 * Game launcher: validates the item (engine exists, game file present),
 * creates the save directory, expands the command and spawns it. The
 * process is then polled non-blockingly by the LAUNCHING screen so the
 * UI keeps animating and hotplug events keep flowing.
 */

#include "engine/engine_registry.h"
#include "engine/process.h"
#include "library/library_item.h"

typedef enum launch_result {
    LAUNCH_OK = 0,
    LAUNCH_ERR_ENGINE_MISSING,     /* engine id unknown          */
    LAUNCH_ERR_GAME_MISSING,       /* game file not on media     */
    LAUNCH_ERR_VARIABLES,          /* expansion overflow         */
    LAUNCH_ERR_SPAWN               /* fork/exec failed           */
} launch_result_t;

typedef struct launcher {
    proc   p;
    int    active;            /* a game was requested            */
    launch_result_t last_error;
    char   error_detail[256]; /* extra info for the toast        */
} launcher;

void launcher_init(launcher *l);

/* Start the item. Returns LAUNCH_OK when the process is running.
 * On failure nothing is spawned and last_error/error_detail are set. */
launch_result_t launcher_start(launcher *l, const library_item *item,
                               const engine_registry *engines,
                               const char *saves_base);

/* Poll the child; returns 1 while running, 0 when finished/idle. */
int launcher_update(launcher *l);

#endif /* DISCADIA_LAUNCHER_H */
