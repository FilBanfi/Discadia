#ifndef DISCADIA_AUDIO_H
#define DISCADIA_AUDIO_H

/*
 * Audio system: thin SDL2_mixer wrapper for UI navigation sounds.
 * Sounds come from the active theme (theme.sounds). Missing files are
 * replaced by procedurally synthesized beeps so a theme without any
 * WAV still produces feedback. Opening the device is best-effort: if
 * there is no sound card the whole module degrades to silent no-ops
 * and never fails the application.
 */

typedef struct audio audio;

/* Open the mixer device. Returns the handle even on failure (silent). */
audio *audio_create(int frequency);
void   audio_destroy(audio *a);

/* Global enable/volume (0..1) driven by settings + theme. */
void audio_set_enabled(audio *a, int enabled);
void audio_set_volume(audio *a, float volume); /* 0..1 */
int  audio_is_enabled(const audio *a);

/* Load/reload the sound set. `sound_dir_paths`/`names` come straight
 * from the theme (parallel arrays, count entries). Logical names used
 * by the UI: "navigate", "select", "back", "cd_insert", "boot".
 * Any name missing on disk gets a synthesized fallback tone. */
void audio_reload_sounds(audio *a, const char **paths, const char **names,
                         int count);

/* Play a logical sound by name ("navigate"...). No-op when disabled or
 * unknown. Never blocks the frame. */
void audio_play(audio *a, const char *logical_name);

#endif /* DISCADIA_AUDIO_H */
