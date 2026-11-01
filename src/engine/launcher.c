/*
 * launcher.c - see launcher.h
 */

#include "engine/launcher.h"
#include "engine/variables.h"
#include "util/fs.h"
#include "util/string_util.h"
#include "log.h"

#include <stdio.h>
#include <string.h>

void launcher_init(launcher *l)
{
    if (!l) return;
    memset(l, 0, sizeof(*l));
}

launch_result_t launcher_start(launcher *l, const library_item *item,
                               const engine_registry *engines,
                               const char *saves_base)
{
    launcher_init(l);
    if (!l || !item) return LAUNCH_ERR_SPAWN;

    const engine *eng = engine_registry_find(engines, item->engine);
    if (!eng)
    {
        l->last_error = LAUNCH_ERR_ENGINE_MISSING;
        str_safe_copy(l->error_detail, item->engine, sizeof(l->error_detail));
        return l->last_error;
    }

    if (item->game_path[0] == '\0' || !fs_file_exists(item->game_path))
    {
        l->last_error = LAUNCH_ERR_GAME_MISSING;
        str_safe_copy(l->error_detail, item->game_path, sizeof(l->error_detail));
        return l->last_error;
    }

    var_ctx vc;
    if (vars_build(&vc, eng, item, saves_base) != 0)
    {
        l->last_error = LAUNCH_ERR_VARIABLES;
        return l->last_error;
    }

    if (fs_mkdir_p(vc.save_dir, 0755) != 0)
        LOG_WARN("could not create save dir %s", vc.save_dir); /* non-fatal */

    char cmdline[ENGINE_MAX_COMMAND + 1024];
    if (vars_expand(&vc, eng->command, cmdline, sizeof(cmdline)) != 0)
    {
        l->last_error = LAUNCH_ERR_VARIABLES;
        return l->last_error;
    }

    char argbuf[2048];
    char *argv[PROC_MAX_ARGS];
    int argc = proc_split_command(cmdline, argbuf, sizeof(argbuf),
                                  argv, PROC_MAX_ARGS);
    if (argc <= 0)
    {
        l->last_error = LAUNCH_ERR_VARIABLES;
        str_safe_copy(l->error_detail, cmdline, sizeof(l->error_detail));
        return l->last_error;
    }

    LOG_INFO("launching '%s': %s", item->title, cmdline);
    if (proc_spawn(&l->p, argv) != 0)
    {
        l->last_error = LAUNCH_ERR_SPAWN;
        return l->last_error;
    }
    l->active = 1;
    l->last_error = LAUNCH_OK;
    return LAUNCH_OK;
}

int launcher_update(launcher *l)
{
    if (!l || !l->active) return 0;
    if (proc_poll(&l->p)) return 1;      /* still running */
    l->active = 0;
    return 0;
}
