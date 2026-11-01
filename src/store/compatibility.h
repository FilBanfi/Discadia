#ifndef DISCADIA_COMPATIBILITY_H
#define DISCADIA_COMPATIBILITY_H

/*
 * Hardware compatibility check: compares a package's compatible_hardware
 * list against the running machine architecture (system_info.c). An
 * empty list means "compatible with everything".
 */

#include "store/store_model.h"

int compat_check_pkg(const store_pkg *pkg);      /* 1 ok, 0 no */
int compat_arch_supported(const char *arch);     /* single token */

#endif /* DISCADIA_COMPATIBILITY_H */
