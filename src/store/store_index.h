#ifndef DISCADIA_STORE_INDEX_H
#define DISCADIA_STORE_INDEX_H

/*
 * Parse a store index.json into the model (see store-repo/index.json).
 * Multilingual fields follow the same rule as game metadata:
 * "description_<lang>" preferred, then "description_en", then bare key.
 */

#include "store/store_model.h"

/* Parse from an in-memory JSON string. Returns 0 on success, -1 bad input. */
int store_index_parse(const char *json_text, store_model *out);

/* Parse directly from a file path. Returns 0 on success. */
int store_index_parse_file(const char *path, store_model *out);

#endif /* DISCADIA_STORE_INDEX_H */
