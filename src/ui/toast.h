#ifndef DISCADIA_TOAST_H
#define DISCADIA_TOAST_H

/*
 * Toast notifications: small themed banners shown at the top of the
 * screen for a configurable timeout (theme.toast_timeout_ms). Used for
 * every non-fatal feedback: disc inserted/removed, controller hotplug,
 * install results, errors. Screens never crash on toast failures and
 * toasts never block input.
 */

#include "ui/widgets.h"

typedef struct toast_queue toast_queue;

toast_queue *toast_queue_create(void);
void         toast_queue_destroy(toast_queue *q);

/* Push a message with default timeout. text must be UTF-8. */
void toast_push(toast_queue *q, const char *text);

/* Push an error-styled toast (accent border). */
void toast_push_error(toast_queue *q, const char *text);

/* Advance timers (call once per frame with dt in ms). Drops expired ones. */
void toast_update(toast_queue *q, float dt_ms);

/* Draw the active toast stack over everything else. */
void toast_draw(toast_queue *q, widget_ctx *w, int sw);

/* Number of pending toasts (debug/tests). */
int toast_pending(const toast_queue *q);

/* Override the default toast lifetime (app sets this from the theme). */
void toast_set_default_ttl_ms(long ms);

#endif /* DISCADIA_TOAST_H */
