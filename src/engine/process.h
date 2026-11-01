#ifndef DISCADIA_PROCESS_H
#define DISCADIA_PROCESS_H

/*
 * Process management: split a command line into argv (shell-like quoting,
 * no eval), fork+exec it, and wait non-blockingly so the UI stays alive.
 */

#include <sys/types.h>
#include <stddef.h>

#define PROC_MAX_ARGS 64

typedef struct proc {
    int   running;
    pid_t pid;
    int   exit_code;     /* valid once finished */
    int   signaled;
} proc;

/* Tokenize `cmdline` honoring single/double quotes. Fills argv (owned by
 * caller arrays) and returns argc, or -1 on overflow/bad quoting. */
int proc_split_command(const char *cmdline, char *buf, size_t buf_size,
                       char *argv[], int argv_max);

/* Fork/exec argv in the child (with stdio inherited so the game can
 * print). Returns 0 on spawn success with p->pid set. */
int proc_spawn(proc *p, char *const argv[]);

/* Poll a running process without blocking. Sets running=0 and captures
 * exit status when it finishes. Returns 1 if still running, 0 done. */
int proc_poll(proc *p);

#endif /* DISCADIA_PROCESS_H */
