#ifndef DISCADIA_ENGINE_H
#define DISCADIA_ENGINE_H

/*
 * Engine abstraction: an engine is a JSON file describing how to launch
 * a game (command template, save dir template, supported extensions).
 * Engines live in the engines folder of data_dir and are loaded at boot by
 * the registry. Nothing in the launcher hardcodes an emulator name.
 */

#define ENGINE_MAX_COMMAND   1024
#define ENGINE_MAX_EXT       8

typedef struct engine {
    char id[64];
    char name[128];
    char command[ENGINE_MAX_COMMAND];        /* with {var} placeholders  */
    char save_dir_template[512];             /* with {var} placeholders  */
    char platform[64];
    char extensions[ENGINE_MAX_EXT][16];     /* ".iso", ".chd", ...      */
    int  extension_count;
} engine;

#endif /* DISCADIA_ENGINE_H */

/* True when `file` has one of the engine's declared extensions
 * (case-insensitive). No extension list -> accepts anything. */
int engine_accepts_file(const engine *eng, const char *file);
