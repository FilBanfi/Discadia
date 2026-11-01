#ifndef DISCADIA_LIBRARY_ITEM_H
#define DISCADIA_LIBRARY_ITEM_H

/*
 * One playable entry in the library: a game/app from an installed folder
 * or from a mounted disc/USB. Titles and descriptions are multilingual:
 * metadata resolution prefers "<key>_<lang>", then "<key>_en", then the
 * bare "<key>" (implemented in metadata.c).
 */

#include <stddef.h>

typedef enum item_source {
    SRC_INSTALLED = 0,   /* ~/.local/share/discadia/games/<id>/        */
    SRC_DISC            /* /media/discadia/disc<N>/                    */
} item_source_t;

typedef struct library_item {
    char id[128];
    char title[256];            /* resolved for current language       */
    char description[1024];     /* resolved for current language       */
    char platform[64];
    char publisher[128];
    char developer[128];
    char genre[64];
    char version[32];
    char region[32];
    int  year;
    int  players;

    char engine[128];           /* engine id from metadata.json        */
    char game_file[512];        /* file name on media (may be empty)   */
    char thumbnail[512];        /* file name (relative to metadata dir)*/

    char metadata_dir[512];     /* folder containing metadata.json     */
    char game_path[768];        /* absolute path of the launchable     */
    item_source_t source;
    int  drive_index;           /* -1 when not from disc               */
} library_item;

/* Parse metadata.json at dir into item. Fills title/description using
 * the current locale. Returns 0 on success, -1 on missing/bad file. */
int library_item_load_metadata(library_item *item, const char *dir,
                               item_source_t source, int drive_index);

/* Human "platform • year • ..." line built per theme metadata_display
 * flags by the caller; this helper only formats one field pair. */
void library_item_meta_line(const library_item *it, char *dst, size_t dst_size,
                            const int show[8], const char *separator);

#endif /* DISCADIA_LIBRARY_ITEM_H */
