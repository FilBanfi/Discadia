/*
 * power.c - see power.h. Never exits() directly: we spawn the init
 * system's own tools so it can run its normal teardown (sync, unmount).
 */

#include "power.h"
#include "log.h"

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

static int run_cmd(const char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0)
    {
        execv(argv[0], (char *const *)argv);
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return 0;
    return -1;
}

static int try_power(const char *systemctl_arg, const char *fallback_bin)
{
    const char *syscmd[] = { "/bin/systemctl", systemctl_arg, NULL };
    if (access("/bin/systemctl", X_OK) == 0 && run_cmd(syscmd) == 0)
        return 0;
    const char *legacy[] = { fallback_bin, NULL };
    if (access(fallback_bin, X_OK) == 0 && run_cmd(legacy) == 0)
        return 0;
    LOG_ERROR("power: neither systemctl nor %s worked", fallback_bin);
    return -1;
}

int power_shutdown(void) { return try_power("poweroff", "/sbin/poweroff"); }
int power_reboot(void)   { return try_power("reboot",   "/sbin/reboot"); }
