#ifndef DISCADIA_LOG_H
#define DISCADIA_LOG_H

/*
 * Mini logger globale (header-only, zero overhead quando disabilitato).
 * Abilitato con la variabile d'ambiente DISCADIA_DEBUG=1.
 * Formato: [LEVEL] YYYY-MM-DD HH:MM:SS - messaggio
 */

#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Ritorna 1 se il logging e' abilitato (DISCADIA_DEBUG=1). */
int discadia_debug_enabled(void);

/* Scrive una riga di log al livello indicato ("INFO", "DEBUG", "WARN", "ERROR"). */
void discadia_log_write(const char *level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#define LOG_I(...)  do { if (discadia_debug_enabled()) discadia_log_write("INFO",  __VA_ARGS__); } while (0)
#define LOG_D(...)  do { if (discadia_debug_enabled()) discadia_log_write("DEBUG", __VA_ARGS__); } while (0)
#define LOG_W(...)  discadia_log_write("WARN",  __VA_ARGS__)
#define LOG_E(...)  discadia_log_write("ERROR", __VA_ARGS__)

#define LOG_INFO(...)  do { if (discadia_debug_enabled()) discadia_log_write("INFO",  __VA_ARGS__); } while (0)
#define LOG_DEBUG(...) do { if (discadia_debug_enabled()) discadia_log_write("DEBUG", __VA_ARGS__); } while (0)
#define LOG_WARN(...)  discadia_log_write("WARN",  __VA_ARGS__)
#define LOG_ERROR(...) discadia_log_write("ERROR", __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* DISCADIA_LOG_H */
