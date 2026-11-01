#ifndef DISCADIA_NETWORK_STATUS_H
#define DISCADIA_NETWORK_STATUS_H

/*
 * Connectivity check used before hitting the store. Reads /sys/class/net
 * for any non-loopback interface with carrier=1: zero cost, no DNS, no
 * sockets. Result is cached for a couple of seconds so per-frame calls
 * are free.
 */

int network_is_online(void);   /* 1 online, 0 offline */

#endif /* DISCADIA_NETWORK_STATUS_H */
