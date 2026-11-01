#ifndef DISCADIA_THEME_PARSER_H
#define DISCADIA_THEME_PARSER_H

#include "theme.h"

/* Parse a theme.json file into the theme struct (colors, sizes, layout,
 * animations, effects, sounds, metadata_display). Relative asset paths in
 * the JSON are resolved by theme_resolve_assets(). Returns 0 on success. */
int theme_parse_json(discadia_theme *theme, const char *theme_json_path);

/* Resolve relative font/sound/asset paths against theme->root_path and
 * verify the files exist; missing entries are left empty so callers can
 * fall back to procedural/system resources. */
void theme_resolve_assets(discadia_theme *theme);

#endif /* DISCADIA_THEME_PARSER_H */
