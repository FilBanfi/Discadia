/*
 * audio.c - see audio.h. Best-effort SDL2_mixer device + themed sounds.
 * Every public entry point is NULL-safe and never returns errors to the
 * UI layer: audio failure must never stop the launcher.
 */

#include "audio/audio.h"
#include "audio/sound_cache.h"
#include "log.h"

#include <SDL.h>
#include <SDL_mixer.h>
#include <stdlib.h>
#include <string.h>

struct audio {
    sound_cache cache;
    int         open;      /* mixer initialized */
    int         enabled;
    float       volume;    /* 0..1 */
};

/* Default fallback tones per logical name (Hz, ms). */
static void tone_for(const char *name, int *freq, int *dur)
{
    if (!strcmp(name, "navigate"))  { *freq = 660;  *dur = 45;  }
    else if (!strcmp(name, "select"))   { *freq = 880;  *dur = 90;  }
    else if (!strcmp(name, "back"))     { *freq = 440;  *dur = 70;  }
    else if (!strcmp(name, "cd_insert")){ *freq = 1200; *dur = 140; }
    else                                { *freq = 520;  *dur = 80;  }
}

audio *audio_create(int frequency)
{
    audio *a = calloc(1, sizeof(*a));
    if (!a) return NULL;
    sound_cache_init(&a->cache);
    a->enabled = 1;                 /* ON by default per project decision */
    a->volume = 0.7f;

    if (Mix_OpenAudio(frequency > 0 ? frequency : 44100,
                      MIX_DEFAULT_FORMAT, 2, 512) == 0)
    {
        a->open = 1;
        Mix_Volume(-1, MIX_MAX_VOLUME);
    }
    else
    {
        LOG_INFO("no audio device (%s): running silent", Mix_GetError());
    }
    return a;
}

void audio_destroy(audio *a)
{
    if (!a) return;
    sound_cache_clear(&a->cache);
    if (a->open) Mix_CloseAudio();
    free(a);
}

void audio_set_enabled(audio *a, int enabled)
{
    if (a) a->enabled = enabled ? 1 : 0;
}

void audio_set_volume(audio *a, float volume)
{
    if (!a) return;
    if (volume < 0.f) volume = 0.f;
    if (volume > 1.f) volume = 1.f;
    a->volume = volume;
    if (a->open) Mix_Volume(-1, (int)(volume * MIX_MAX_VOLUME));
}

int audio_is_enabled(const audio *a)
{
    return a && a->enabled && a->open;
}

void audio_reload_sounds(audio *a, const char **paths, const char **names,
                         int count)
{
    if (!a) return;
    sound_cache_clear(&a->cache);
    for (int i = 0; i < count && i < SC_MAX_SOUNDS; i++)
    {
        if (!names[i]) continue;
        int f, d;
        tone_for(names[i], &f, &d);
        sound_cache_load(&a->cache, names[i], paths ? paths[i] : NULL, f, d);
    }
}

void audio_play(audio *a, const char *logical_name)
{
    if (!audio_is_enabled(a) || !logical_name) return;
    Mix_Chunk *c = sound_cache_get(&a->cache, logical_name);
    if (!c) return;
    /* CHANNEL-1: let the mixer pick; rapid navigation stays snappy. */
    Mix_PlayChannel(-1, c, 0);
}
