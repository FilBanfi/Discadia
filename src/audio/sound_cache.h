#ifndef DISCADIA_SOUND_CACHE_H
#define DISCADIA_SOUND_CACHE_H

/*
 * Sound cache: bounded set of Mix_Chunk effects keyed by logical name.
 * Loads WAV files once; synthesizes short tones when the file is absent
 * so themes without audio assets remain functional. Owned by audio.c.
 */

#include <SDL_mixer.h>

#define SC_MAX_SOUNDS 8

typedef struct sound_entry {
    char       name[32];
    Mix_Chunk *chunk;           /* owned */
} sound_entry;

typedef struct sound_cache {
    sound_entry items[SC_MAX_SOUNDS];
    int         count;
} sound_cache;

void sound_cache_init(sound_cache *sc);
/* Free every chunk and reset. */
void sound_cache_clear(sound_cache *sc);
/* Load path under logical name (replaces existing entry). On load
 * failure a tone is synthesized instead (freq_hz/duration_ms). */
void sound_cache_load(sound_cache *sc, const char *name, const char *path,
                      int freq_hz, int duration_ms);
/* Borrowed chunk or NULL. */
Mix_Chunk *sound_cache_get(sound_cache *sc, const char *name);

#endif /* DISCADIA_SOUND_CACHE_H */
