/*
 * sound_cache.c - see sound_cache.h. WAV loading plus procedural tone
 * synthesis (square wave with a fast decay envelope) so navigation
 * feedback exists even for themes that ship no audio assets.
 */

#include "audio/sound_cache.h"
#include "util/string_util.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

void sound_cache_init(sound_cache *sc)
{
    if (sc) memset(sc, 0, sizeof(*sc));
}

void sound_cache_clear(sound_cache *sc)
{
    if (!sc) return;
    for (int i = 0; i < sc->count; i++)
    {
        if (sc->items[i].chunk) Mix_FreeChunk(sc->items[i].chunk);
        sc->items[i].chunk = NULL;
        sc->items[i].name[0] = '\0';
    }
    sc->count = 0;
}

static sound_entry *find_or_add(sound_cache *sc, const char *name)
{
    for (int i = 0; i < sc->count; i++)
        if (strcmp(sc->items[i].name, name) == 0) return &sc->items[i];
    if (sc->count >= SC_MAX_SOUNDS) return NULL;
    sound_entry *e = &sc->items[sc->count++];
    str_safe_copy(e->name, name, sizeof(e->name));
    return e;
}

/* Build a short mono 16-bit tone with exponential decay. */
static Mix_Chunk *synthesize_tone(int freq_hz, int duration_ms)
{
    int rate = 22050;
    Uint32 samples = (Uint32)((long)rate * duration_ms / 1000);
    size_t bytes = (size_t)samples * 2;
    Uint8 *buf = malloc(bytes);
    if (!buf) return NULL;

    int16_t *s = (int16_t *)buf;
    for (Uint32 i = 0; i < samples; i++)
    {
        double t = (double)i / rate;
        double env = exp(-6.0 * t * 1000.0 / duration_ms);
        /* soft square: sum of fundamental + third harmonic */
        double v = sin(2 * M_PI * freq_hz * t) +
                   0.33 * sin(2 * M_PI * freq_hz * 3 * t);
        s[i] = (int16_t)(v * env * 12000.0);
    }
    SDL_RWops *rw = SDL_RWFromMem(buf, (int)bytes);
    Mix_Chunk *chunk = NULL;
    if (rw)
    {
        chunk = Mix_LoadWAV_RW(rw, 0); /* don't free buf here yet */
        SDL_RWclose(rw);
    }
    if (!chunk)
    {
        /* Fallback path: build the header manually (RWFromMem WAV parse
         * can fail on exotic builds). Write a canonical 44-byte header. */
        size_t total = 44 + bytes;
        Uint8 *wav = calloc(1, total);
        if (!wav) { free(buf); return NULL; }
        memcpy(wav, "RIFF", 4);
        Uint32 riff = (Uint32)(total - 8);   memcpy(wav + 4, &riff, 4);
        memcpy(wav + 8, "WAVEfmt ", 8);
        Uint32 csz = 16;                     memcpy(wav + 16, &csz, 4);
        Uint16 fmt = 1, ch = 1;              memcpy(wav + 20, &fmt, 2);
                                           memcpy(wav + 22, &ch, 2);
        Uint32 sr = rate;                    memcpy(wav + 24, &sr, 4);
        Uint32 brate = rate * 2;             memcpy(wav + 28, &brate, 4);
        Uint16 ba = 2, align = 2;            memcpy(wav + 32, &ba, 2);
                                           memcpy(wav + 34, &align, 2);
        Uint32 dlen = (Uint32)bytes;         memcpy(wav + 36, "data", 4);
                                           memcpy(wav + 40, &dlen, 4);
        memcpy(wav + 44, buf, bytes);
        rw = SDL_RWFromMem(wav, (int)total);
        if (rw)
        {
            chunk = Mix_LoadWAV_RW(rw, 0);
            SDL_RWclose(rw);
        }
        free(wav);
    }
    free(buf);
    return chunk;
}

void sound_cache_load(sound_cache *sc, const char *name, const char *path,
                      int freq_hz, int duration_ms)
{
    if (!sc || !name) return;
    sound_entry *e = find_or_add(sc, name);
    if (!e) return;
    if (e->chunk) { Mix_FreeChunk(e->chunk); e->chunk = NULL; }

    if (path && *path)
    {
        e->chunk = Mix_LoadWAV(path);
        if (e->chunk) { LOG_DEBUG("sound loaded: %s -> %s", name, path); return; }
        LOG_DEBUG("sound file unreadable, synthesizing: %s", path);
    }
    e->chunk = synthesize_tone(freq_hz > 0 ? freq_hz : 880,
                               duration_ms > 0 ? duration_ms : 90);
}

Mix_Chunk *sound_cache_get(sound_cache *sc, const char *name)
{
    if (!sc || !name) return NULL;
    for (int i = 0; i < sc->count; i++)
        if (strcmp(sc->items[i].name, name) == 0) return sc->items[i].chunk;
    return NULL;
}
