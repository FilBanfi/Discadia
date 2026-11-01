/*
 * engine.c - engine struct helpers. The struct itself is plain data
 * (engine.h); this TU keeps shared utilities for validation so other
 * modules never reimplement extension matching.
 */

#include "engine/engine.h"
#include "util/string_util.h"

#include <string.h>
#include <ctype.h>

static void path_extension_safe(char *dst, size_t size, const char *path)
{
    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    if (!dot || (slash && dot < slash)) { dst[0] = '\0'; return; }
    str_safe_copy(dst, dot, size);
}

/* Does `file` carry one of the engine's declared extensions?
 * Case-insensitive. Engines with no extension list accept anything. */
int engine_accepts_file(const engine *eng, const char *file)
{
    if (!eng || !file) return 0;
    if (eng->extension_count <= 0) return 1;

    char ext[16];
    path_extension_safe(ext, sizeof(ext), file);
    for (int i = 0; i < eng->extension_count; i++)
        if (strcasecmp(ext, eng->extensions[i]) == 0) return 1;
    return 0;
}

