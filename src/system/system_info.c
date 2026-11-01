/*
 * system_info.c - see system_info.h
 */

#include "system_info.h"
#include "util/fs.h"
#include "util/string_util.h"
#include "log.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

static char s_arch[32]    = "";
static char s_model[128]  = "";

static void detect_arch(void)
{
#if defined(__aarch64__)
    str_safe_copy(s_arch, "arm64", sizeof(s_arch));
#elif defined(arm) || defined(__arm__)
    /* Distinguish armv6/v7 via /proc/cpuinfo Features line. */
    char *cpu = fs_read_file("/proc/cpuinfo", NULL);
    if (cpu && strstr(cpu, "neon"))
        str_safe_copy(s_arch, "armv7", sizeof(s_arch));
    else if (cpu && strstr(cpu, "v6"))
        str_safe_copy(s_arch, "armv6", sizeof(s_arch));
    else
        str_safe_copy(s_arch, "armv7", sizeof(s_arch));
    free(cpu);
#elif defined(__x86_64__)
    str_safe_copy(s_arch, "x86_64", sizeof(s_arch));
#elif defined(__i386__)
    str_safe_copy(s_arch, "i686", sizeof(s_arch));
#else
    str_safe_copy(s_arch, "unknown", sizeof(s_arch));
#endif
}

static void detect_model(void)
{
    /* Device tree model (Raspberry Pi, most ARM SBCs). */
    char *dt = fs_read_file("/sys/firmware/devicetree/base/model", NULL);
    if (dt)
    {
        str_safe_copy(s_model, dt, sizeof(s_model));
        free(dt);
        str_trim(s_model);
        if (s_model[0]) return;
    }
    /* Fallback: first "Model :" line of /proc/cpuinfo. */
    char *cpu = fs_read_file("/proc/cpuinfo", NULL);
    if (cpu)
    {
        char *line = strstr(cpu, "Model");
        if (line)
        {
            char *colon = strchr(line, ':');
            if (colon)
            {
                colon++;
                while (*colon == ' ' || *colon == '\t') colon++;
                char *eol = strchr(colon, '\n');
                size_t n = eol ? (size_t)(eol - colon) : strlen(colon);
                if (n >= sizeof(s_model)) n = sizeof(s_model) - 1;
                memcpy(s_model, colon, n);
                s_model[n] = '\0';
            }
        }
        free(cpu);
        if (s_model[0]) return;
    }
    if (gethostname(s_model, sizeof(s_model) - 1) != 0)
        str_safe_copy(s_model, "Linux device", sizeof(s_model));
}

const char *sysinfo_arch(void)
{
    if (!s_arch[0]) detect_arch();
    return s_arch;
}

const char *sysinfo_model(void)
{
    if (!s_model[0]) detect_model();
    return s_model;
}

long sysinfo_total_ram_mb(void)
{
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGESIZE);
    if (pages <= 0 || page_size <= 0) return 0;
    return (pages / 1024) * (page_size / 1024);
}
