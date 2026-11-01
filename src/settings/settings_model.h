#ifndef DISCADIA_SETTINGS_MODEL_H
#define DISCADIA_SETTINGS_MODEL_H

/*
 * User settings (persisted in ~/.config/discadia/config.json).
 * The model holds plain values with sane defaults; the store module
 * handles load/save with atomic writes. Screens mutate the model and
 * call settings_store_save() after each change.
 */

typedef enum { CLOCK_FORMAT_24H = 0, CLOCK_FORMAT_12H = 1 } clock_format_t;

typedef struct settings_model {
    int version;                 /* schema version for forward compat */

    char theme[64];              /* theme folder name */
    char language[16];           /* locale code       */

    int  audio_enabled;          /* navigation sounds ON by default */
    float audio_volume;          /* 0..1 */

    int animations_enabled;

    int show_clock;
    clock_format_t clock_format;
    int confirm_shutdown;

    /* Store */
    int store_auto_update_index; /* refresh index on store open */
    int store_cache_days;        /* max age of cached index before refetch */
} settings_model;

/* Fill with defaults: theme "arcade-classic", language "en", audio on 0.7,
 * animations on, clock on 24h, confirm shutdown on, cache 7 days. */
void settings_set_defaults(settings_model *m);

#endif /* DISCADIA_SETTINGS_MODEL_H */
