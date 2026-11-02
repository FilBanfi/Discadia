/*
 * compatibility.c - match package compatible_hardware tokens against the
 * running architecture (system_info). Empty list = compatible with all.
 */

#include <string.h>
#include "store/compatibility.h"
#include "system/system_info.h"

int compat_arch_supported(const char *arch)
{
    if (!arch || !*arch) return 1;
    const char *mine = sysinfo_arch();   /* e.g. "arm64", "x86_64", "armv7l" */
    if (!mine || !*mine) return 1;       /* unknown machine -> allow        */
    if (!strcmp(arch, mine)) return 1;
    /* Common aliases: arm == armv6/armv7/armv7l/armhf, aarch64 == arm64. */
    if (!strcmp(arch, "arm") && strstr(mine, "arm")) return 1;
    if (!strcmp(arch, "armv7") && !strncmp(mine, "armv7", 5)) return 1;
    if (!strcmp(arch, "arm64") && !strcmp(mine, "aarch64")) return 1;
    if (!strcmp(arch, "aarch64") && !strcmp(mine, "arm64")) return 1;
    if (!strcmp(arch, "x86") && !strcmp(mine, "i686")) return 1;
    return 0;
}

int compat_check_pkg(const store_pkg *pkg)
{
    if (!pkg) return 0;
    if (pkg->arch_count <= 0) return 1;      /* no restriction listed */
    for (int i = 0; i < pkg->arch_count; i++)
        if (compat_arch_supported(pkg->archs[i])) return 1;
    return 0;
}
