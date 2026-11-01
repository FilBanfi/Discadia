#include "string_util.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

void str_safe_copy(char *dst, const char *src, size_t dst_size) {
    if (dst == NULL || dst_size == 0) return;
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

void str_safe_cat(char *dst, const char *src, size_t dst_size) {
    if (dst == NULL || dst_size == 0) return;
    if (src == NULL) return;
    size_t dst_len = strlen(dst);
    if (dst_len >= dst_size - 1) return;
    strncat(dst, src, dst_size - dst_len - 1);
}

char *str_duplicate(const char *src) {
    if (src == NULL) return NULL;
    size_t len = strlen(src) + 1;
    char *dup = malloc(len);
    if (dup == NULL) return NULL;
    memcpy(dup, src, len);
    return dup;
}

void str_trim(char *s) {
    if (s == NULL || *s == '\0') return;
    
    // Trim leading
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
    
    // Trim trailing
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

void str_to_lower(char *s) {
    if (s == NULL) return;
    for (; *s; s++) {
        *s = (char)tolower((unsigned char)*s);
    }
}

int str_starts_with(const char *s, const char *prefix) {
    if (s == NULL || prefix == NULL) return 0;
    size_t prefix_len = strlen(prefix);
    return strncmp(s, prefix, prefix_len) == 0;
}

int str_ends_with(const char *s, const char *suffix) {
    if (s == NULL || suffix == NULL) return 0;
    size_t s_len = strlen(s);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > s_len) return 0;
    return strcmp(s + s_len - suffix_len, suffix) == 0;
}

int str_is_empty(const char *s) {
    return s == NULL || *s == '\0';
}

int str_format(char *dst, size_t size, const char *fmt, ...) {
    if (dst == NULL || size == 0 || fmt == NULL) return -1;
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(dst, size, fmt, args);
    va_end(args);
    dst[size - 1] = '\0';
    return ret;
}
