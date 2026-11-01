#ifndef DISCADIA_SCANNER_H
#define DISCADIA_SCANNER_H

/*
 * Library scanner: walks the installed-games root
 * (~/.local/share/discadia/games) one level deep, loading every folder
 * that contains a metadata.json. Disc media are handled by the media
 * module (disc_scanner.c) which feeds library_add() directly.
 */

#include "library/library.h"

/* Scan installed games and append them to lib (old installed entries
 * are dropped first; disc entries are preserved). Returns item count
 * added or -1 on error. */
int scanner_scan_installed(library *lib);

#endif /* DISCADIA_SCANNER_H */
