#ifndef DISCADIA_LOCALE_H

#include <stddef.h>
#define DISCADIA_LOCALE_H

/*
 * i18n: every user-visible string goes through locale_get(). No UI code
 * may contain literal display strings. Missing keys return the key itself
 * (never crash, never show empty text). Default language is "en".
 */

/* Load locales from <data_dir>/locales/{code}.json. Falls back to "en"
 * when the requested file is missing or malformed. Returns 0 on success,
 * -1 only if even English could not be loaded. */
int locale_init(const char *data_dir, const char *lang_code);

/* Get translated string by key. Returns the key itself if not found. */
const char *locale_get(const char *key);

/* Translate a format string with up to 3 named placeholders, e.g.
 * locale_fmt("error_engine_not_found", "engine", "pcsx"); replaces
 * occurrences of {name} in the template. Writes into out buffer. */
void locale_fmt(char *out, size_t out_size, const char *key,
                const char *ph_name1, const char *ph_val1,
                const char *ph_name2, const char *ph_val2);

/* Current language code ("en", "it", ...). Never NULL. */
const char *locale_get_current(void);

/* Switch language at runtime (reload JSON). Same fallback rules as init. */
int locale_set(const char *data_dir, const char *lang_code);

/* List available languages: scans locales dir of data_dir and calls
 * cb(code, native_name) for each. Returns count or -1. */
typedef int (*locale_list_cb)(const char *code, const char *native_name,
                              void *userdata);
int locale_list_available(const char *data_dir, locale_list_cb cb, void *userdata);

#endif /* DISCADIA_LOCALE_H */
