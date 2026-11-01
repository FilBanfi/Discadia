#ifndef DISCADIA_STRING_UTIL_H
#define DISCADIA_STRING_UTIL_H

#include <stddef.h>

// Safe string copy (always null-terminates)
void str_safe_copy(char *dst, const char *src, size_t dst_size);

// Safe string concatenation (always null-terminates)
void str_safe_cat(char *dst, const char *src, size_t dst_size);

// Duplicate a string (caller must free)
char *str_duplicate(const char *src);

// Trim whitespace from both ends (modifies in place)
void str_trim(char *s);

// Convert string to lowercase (in place)
void str_to_lower(char *s);

// Check if string starts with prefix
int str_starts_with(const char *s, const char *prefix);

// Check if string ends with suffix
int str_ends_with(const char *s, const char *suffix);

// Check if string is empty or NULL
int str_is_empty(const char *s);

// Safe snprintf wrapper (always null-terminates)
int str_format(char *dst, size_t size, const char *fmt, ...);

#endif
