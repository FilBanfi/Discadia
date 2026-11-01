/*
 * main.c - Discadia entry point.
 *
 * Responsibilities are deliberately tiny: parse the two environment
 * switches, build the application context, run the loop, clean up.
 * All behavior lives in src/app/.
 *
 * Environment:
 *   DISCADIA_DEBUG=1       verbose logging + FPS counter + layout borders
 *   DISCADIA_DATA_DIR=DIR  override data dir (themes/locales/engines)
 *   DISCADIA_MEDIA_DIR=DIR override disc mount root (default /media/discadia)
 *   DISCADIA_VIDEODRIVER   force an SDL video driver (e.g. dummy for CI)
 *   DISCADIA_STORE_URL     override store index base URL
 */

#include "app/app.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    LOG_INFO("Discadia %s starting", app_version());

    app *a = app_create();
    if (!a)
    {
        fprintf(stderr, "discadia: out of memory\n");
        return 1;
    }
    if (app_init(a) != 0)
    {
        fprintf(stderr, "discadia: initialization failed "
                        "(check SDL2 installation / display)\n");
        app_destroy(a);
        return 1;
    }

    app_run(a);          /* blocks until quit/shutdown request */

    app_shutdown(a);
    app_destroy(a);
    LOG_INFO("bye");
    return 0;
}
