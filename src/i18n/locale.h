#ifndef DISCADIA_LOCALE_H
#define DISCADIA_LOCALE_H

// Initialize locale system with given language code (e.g., "it", "en")
// Loads data/locales/{code}.json
int locale_init(const char *lang_code);

// Get translated string by key. Returns key if not found.
const char *locale_get(const char *key);

// Get current language code
const char *locale_get_current(void);

// Change language at runtime
int locale_set(const char *lang_code);

// Cleanup
void locale_cleanup(void);

#endif
