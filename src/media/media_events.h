#ifndef DISCADIA_MEDIA_EVENTS_H
#define DISCADIA_MEDIA_EVENTS_H

/*
 * Media event integration points. The shipped implementation is pure
 * polling inside disc_manager (works everywhere, zero dependencies).
 * The udev rule in config/99-discadia-disc.rules only needs to mount
 * discs under /media/discadia/<name>/ - which is exactly what the
 * manager watches - so no socket listener is required here. This header
 * exists as the documented extension point for future netlink support.
 */

#endif /* DISCADIA_MEDIA_EVENTS_H */
