/*
 * compatibility.c - package/hardware compatibility checks.
 *
 * A package with an empty compatible_hardware list is assumed to run
 * everywhere (most themes, engines built portable, etc.). Otherwise the
 * running architecture from sysinfo_arch() must appear in the list.
 *
 * Architecture aliases: store authors may write "aarch64" where uname
 * gives "arm64", or "armhf"/"armv7hl" for "armv7". We normalize both
 * sides before comparing so manifests stay forgiving.
 */

#include "store/compatibility.h"
#include "system/system_info.h"
#include "util/string_util.h"

#include <string.h>
#include <strings.h> /* strcasecmp */

/* Map common aliases onto our canonical arch tokens. Static table:
 * this runs on devices without much RAM and never allocates. */
static const struct { const char *alias; const char *canon; } ARCH_ALIASES[] = {
    { "aarch64",  "arm64"   },
    { "arm64",    "arm64"   },
    { "armhf",    "armv7"   },
    { "armv7hl",  "armv7"   },
    { "armv7l",   "armv7"   },
    { "armv7",    "armv7"   },
    { "armv6l",   "armv6"   },
    { "armv6",    "armv6"   },
    { "amd64",    "x86_64"  },
    { "x86_64",   "x86_64"  },
    { "x64",      "x86_64"  },
    { "i386",     "i686"    },
    { "i486",     "i686"    },
    { "i586",     "i686"    },
    { "i686",     "i686"    },
    { "x86",      "i686"    },
};
#define ARCH_ALIAS_COUNT ((int)(sizeof(ARCH_ALIASES) / sizeof(ARCH_ALIASES[0])))

static const char *arch_canonical(const char *a)
{
    if (!a || !*a) return "";
    for (int i = 0; i < ARCH_ALIAS_COUNT; i++)
        if (strcasecmp(a, ARCH_ALIASES[i].alias) == 0)
            return ARCH_ALIASES[i].canon;
    return a; /* unknown token: compare verbatim */
}

int compat_arch_supported(const char *arch)
{
    if (!arch || !*arch) return 1; /* empty = wildcard */
    const char *here = arch_canonical(sysinfo_arch());
    const char *want = arch_canonical(arch);
    return strcasecmp(here, want) == 0;
}

int compat_check_pkg(const store_pkg *pkg)
{
    if (!pkg) return 0;
    if (pkg->arch_count <= 0) return 1; /* no constraint */
    for (int i = 0; i < pkg->arch_count; i++)
        if (compat_arch_supported(pkg->archs[i])) return 1;
    return 0;
}
