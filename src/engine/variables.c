/*
 * variables.c - see variables.h
 */

#include "engine/variables.h"
#include "util/path_util.h"
#include "util/string_util.h"
#include "log.h"

#include <stdio.h>
#include <string.h>

static int replace_all(char *dst, size_t cap, const char *src,
                       const char *needle, const char *value)
{
    /* Copy src->dst substituting every occurrence of needle. */
    size_t nlen = strlen(needle), vlen = strlen(value);
    size_t di = 0;
    const char *p = src;
    while (*p)
    {
        if (strncmp(p, needle, nlen) == 0)
        {
            if (di + vlen >= cap) return -1;
            memcpy(dst + di, value, vlen);
            di += vlen;
            p += nlen;
        }
        else
        {
            if (di + 1 >= cap) return -1;
            dst[di++] = *p++;
        }
    }
    dst[di] = '\0';
    return 0;
}

int vars_build(var_ctx *vc, const engine *eng, const library_item *item,
               const char *saves_base)
{
    if (!vc || !item) return -1;
    memset(vc, 0, sizeof(*vc));
    vc->item = item;
    str_safe_copy(vc->saves_base, saves_base ? saves_base : "",
                  sizeof(vc->saves_base));

    const char *tmpl = (eng && eng->save_dir_template[0])
                     ? eng->save_dir_template : "{saves_base}/{game_id}";

    /* Two passes: template may itself reference {saves_base}. */
    char tmp[768];
    if (replace_all(tmp, sizeof(tmp), tmpl, "{saves_base}", vc->saves_base) != 0 ||
        replace_all(vc->save_dir, sizeof(vc->save_dir), tmp, "{game_id}",
                    item->id) != 0)
    {
        LOG_ERROR("vars: save_dir overflow for %s", item->id);
        return -1;
    }
    return 0;
}

int vars_expand(const var_ctx *vc, const char *in, char *out, size_t out_size)
{
    if (!vc || !in || !out || out_size == 0) return -1;
    static const char *needles[] = { "{game_path}", "{game_id}", "{save_dir}",
                                     "{saves_base}", "{metadata_dir}" };
    char acc[2048];
    size_t cap = sizeof(acc);
    if (out_size > cap) cap = out_size;

    if (cap < strlen(in) + 1) return -1;
    str_safe_copy(acc, in, cap);

    const char *vals[5] = { vc->item->game_path, vc->item->id, vc->save_dir,
                            vc->saves_base, vc->item->metadata_dir };
    char scratch[2048];
    for (int i = 0; i < 5; i++)
    {
        if (replace_all(scratch, sizeof(scratch), acc, needles[i],
                        vals[i] ? vals[i] : "") != 0)
        {
            LOG_ERROR("vars: command overflow");
            return -1;
        }
        str_safe_copy(acc, scratch, cap);
    }
    if (strlen(acc) + 1 > out_size) return -1;
    str_safe_copy(out, acc, out_size);
    return 0;
}
