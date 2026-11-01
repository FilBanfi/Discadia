/*
 * theme_assets.c - asset resolution helpers for themes.
 *
 * A theme folder may ship fonts/, sounds/ and assets/. Anything missing
 * degrades gracefully (system font, synthesized sound, procedural art),
 * so these functions never fail hard - they only answer "is it there?".
 */

#include "theme/theme.h"
#include "util/fs.h"
#include "util/path_util.h"
#include "util/string_util.h"
#include "log.h"

#include <string.h>

/* Resolve a relative asset path against the theme root; absolute paths
 * pass through. Returns 1 if the resulting file exists. */
int theme_resolve_asset(const discadia_theme *t, const char *rel,
                       char *dst, size_t dst_size)
{
    if (!t || !rel || !*rel) { if (dst && dst_size) dst[0] = '\0'; return 0; }
    if (path_is_absolute(rel)) str_safe_copy(dst, rel, dst_size);
    else path_join(dst, dst_size, t->root_path, rel);
    return fs_file_exists(dst);
}

/* Full path of an optional frame overlay image (effects.frame_overlay).
 * Empty string when the theme has none. */
void theme_frame_overlay_path(const discadia_theme *t, char *dst, size_t sz)
{
    if (!t || !t->effects.frame_overlay[0] || !t->effects.enabled)
    { if (sz) dst[0] = '\0'; return; }
    if (!theme_resolve_asset(t, t->effects.frame_overlay, dst, sz))
        dst[0] = '\0';
}

/* True when the theme provides its own copy of a logical sound. */
int theme_has_sound_file(const discadia_theme *t, const char *logical_name)
{
    const char *p = theme_sound_path(t, logical_name);
    return p && *p && fs_file_exists(p);
}
