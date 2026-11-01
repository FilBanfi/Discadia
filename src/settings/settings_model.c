/* settings_model.c - defaults only; persistence lives in settings_store.c */

#include "settings_model.h"
#include "util/string_util.h"

void settings_set_defaults(settings_model *m) {
    if (m == NULL) return;
    m->version = 1;
    str_safe_copy(m->theme, "arcade-classic", sizeof(m->theme));
    str_safe_copy(m->language, "en", sizeof(m->language));
    m->audio_enabled = 1;         /* ON by default per user decision */
    m->audio_volume = 0.7f;
    m->animations_enabled = 1;
    m->show_clock = 1;
    m->clock_format = CLOCK_FORMAT_24H;
    m->confirm_shutdown = 1;
    m->store_auto_update_index = 1;
    m->store_cache_days = 7;
}
