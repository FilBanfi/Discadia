#ifndef DISCADIA_ENGINE_REGISTRY_H
#define DISCADIA_ENGINE_REGISTRY_H

/*
 * Engine registry: loads every <data_dir>/engine JSON files at boot and
 * answers lookups by id. Duplicate ids: first file wins (sorted order),
 * later ones are skipped with a warning.
 */

#include "engine/engine.h"

typedef struct engine_registry engine_registry;

engine_registry *engine_registry_create(void);
void             engine_registry_destroy(engine_registry *r);

/* Load all engines from dir (returns count loaded, -1 on opendir fail). */
int engine_registry_load_dir(engine_registry *r, const char *dir);

/* Find an engine by id; NULL when unknown. Borrowed pointer. */
const engine *engine_registry_find(const engine_registry *r, const char *id);

int engine_registry_count(const engine_registry *r);

#endif /* DISCADIA_ENGINE_REGISTRY_H */
