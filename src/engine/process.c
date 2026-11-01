/*
 * process.c - see process.h
 */

#include "engine/process.h"
#include "log.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int proc_split_command(const char *cmdline, char *buf, size_t buf_size,
                       char *argv[], int argv_max)
{
    if (!cmdline || !buf || !argv) return -1;
    size_t bi = 0;
    int argc = 0;
    const char *p = cmdline;

    while (*p)
    {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (argc >= argv_max - 1) return -1;

        argv[argc++] = buf + bi;
        int in_squote = 0, in_dquote = 0;
        while (*p && (in_squote || in_dquote || (*p != ' ' && *p != '\t')))
        {
            if (bi + 1 >= buf_size) return -1;
            char c = *p;
            if (c == '\'' && !in_dquote) { in_squote = !in_squote; }
            else if (c == '"' && !in_squote) { in_dquote = !in_dquote; }
            else
            {
                buf[bi++] = c;
            }
            p++;
        }
        if (in_squote || in_dquote) return -1;  /* unterminated quote */
        buf[bi++] = '\0';
    }
    argv[argc] = NULL;
    return argc;
}

int proc_spawn(proc *p, char *const argv[])
{
    if (!p || !argv || !argv[0]) return -1;
    memset(p, 0, sizeof(*p));

    fflush(NULL);  /* avoid duplicated stdio buffers in the child */
    pid_t pid = fork();
    if (pid < 0)
    {
        LOG_ERROR("fork failed: %s", strerror(errno));
        return -1;
    }
    if (pid == 0)
    {
        execvp(argv[0], argv);
        fprintf(stderr, "[discadia] exec '%s' failed: %s\n",
                argv[0], strerror(errno));
        _exit(127);
    }
    p->pid = pid;
    p->running = 1;
    LOG_INFO("spawned pid %d: %s", (int)pid, argv[0]);
    return 0;
}

int proc_poll(proc *p)
{
    if (!p || !p->running) return 0;
    int status = 0;
    pid_t r = waitpid(p->pid, &status, WNOHANG);
    if (r == 0) return 1;               /* still running */
    p->running = 0;
    if (r < 0)
    {
        /* ECHILD: someone already reaped it; treat as finished ok. */
        p->exit_code = errno == ECHILD ? 0 : -1;
        return 0;
    }
    if (WIFEXITED(status))   p->exit_code = WEXITSTATUS(status);
    if (WIFSIGNALED(status)) { p->signaled = 1; p->exit_code = 128 + WTERMSIG(status); }
    LOG_INFO("process %d finished (code %d%s)", (int)p->pid, p->exit_code,
             p->signaled ? ", signaled" : "");
    return 0;
}
