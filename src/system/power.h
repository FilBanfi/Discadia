#ifndef DISCADIA_POWER_H
#define DISCADIA_POWER_H

/*
 * Power management: requests shutdown/reboot through systemctl when
 * available, otherwise falls back to `poweroff`/`reboot`. The caller is
 * responsible for saving settings and cleaning up first. These calls do
 * not return on success.
 */

int power_shutdown(void);   /* returns -1 if the command could not run */
int power_reboot(void);

#endif /* DISCADIA_POWER_H */
