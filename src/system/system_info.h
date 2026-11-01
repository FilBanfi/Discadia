#ifndef DISCADIA_SYSTEM_INFO_H
#define DISCADIA_SYSTEM_INFO_H

/*
 * Hardware detection: architecture string (for store compatibility),
 * human-readable model name and total RAM. Reads /proc and /sys only,
 * never shells out. All functions are safe on any Linux; unknown values
 * degrade to "unknown"/0 instead of failing.
 */

/* Machine arch as used in store manifests: "armv6","armv7","arm64",
 * "x86_64","i686" or "unknown". Static buffer. */
const char *sysinfo_arch(void);

/* Human model, e.g. "Raspberry Pi Zero W" from /proc/cpuinfo or
 * /sys/firmware/devicetree; falls back to the hostname. Static buffer. */
const char *sysinfo_model(void);

/* Total physical memory in MB (0 if unknown). */
long sysinfo_total_ram_mb(void);

#endif /* DISCADIA_SYSTEM_INFO_H */
