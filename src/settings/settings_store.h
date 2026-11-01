#ifndef DISCADIA_SETTINGS_STORE_H
#define DISCADIA_SETTINGS_STORE_H

#include <stddef.h>
#include "settings_model.h"

/* Load ~/.config/discadia/config.json into the model. Missing file keeps
 * defaults (and returns 0); malformed file falls back to defaults with a
 * warning. Unknown keys are ignored, missing keys keep default values so
 * new settings can be added without breaking old configs. */
int settings_store_load(settings_model *m);

/* Persist the model atomically (tmp + rename + .bak). Returns 0 on success. */
int settings_store_save(const settings_model *m);

/* Full path of the config file (created dir included). */
void settings_store_path(char *dst, size_t dst_size);

#endif /* DISCADIA_SETTINGS_STORE_H */
