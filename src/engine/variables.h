#ifndef DISCADIA_VARIABLES_H
#define DISCADIA_VARIABLES_H

/*
 * Variable substitution for engine commands/templates. Supported vars:
 *   {game_path} {game_id} {save_dir} {saves_base} {metadata_dir}
 * Unknown placeholders are left untouched (and logged in debug) so a
 * typo never silently drops arguments. Values longer than the buffer
 * cause the whole substitution to fail rather than truncate mid-command.
 */

#include "engine/engine.h"
#include "library/library_item.h"

typedef struct var_ctx {
    const library_item *item;
    char saves_base[512];   /* ~/.local/share/discadia/saves          */
    char save_dir[640];     /* resolved from engine template          */
} var_ctx;

/* Build a context for an item+engine: resolves save_dir first.
 * Returns 0 on success, -1 when buffers overflow. */
int vars_build(var_ctx *vc, const engine *eng, const library_item *item,
               const char *saves_base);

/* Expand `in` into `out` replacing every known {var}. Returns 0 ok,
 * -1 on overflow. */
int vars_expand(const var_ctx *vc, const char *in, char *out, size_t out_size);

#endif /* DISCADIA_VARIABLES_H */
