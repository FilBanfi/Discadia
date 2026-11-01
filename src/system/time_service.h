#ifndef DISCADIA_TIME_SERVICE_H
#define DISCADIA_TIME_SERVICE_H

/*
 * Clock formatting for the top bar. Honors the user clock_format
 * setting (24h/12h). No allocations, writes into caller buffers.
 */

typedef enum { TS_24H = 0, TS_12H = 1 } ts_format_t;

/* Current time as "HH:MM" (or "h:MM AM/PM" for 12h). */
void time_now_string(char *dst, int dst_size, ts_format_t fmt);

/* Current date as "YYYY-MM-DD" (used by store cache freshness checks). */
void time_date_string(char *dst, int dst_size);

#endif /* DISCADIA_TIME_SERVICE_H */
