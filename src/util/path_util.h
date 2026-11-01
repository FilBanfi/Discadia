#ifndef DISCADIA_PATH_UTIL_H
#define DISCADIA_PATH_UTIL_H

#include <stddef.h>

// Join two path components with '/' (handles trailing/leading slashes)
void path_join(char *dst, size_t dst_size, const char *a, const char *b);

// Get the directory part of a path (everything before last '/')
void path_dirname(char *dst, size_t dst_size, const char *path);

// Get the filename part of a path (everything after last '/')
void path_basename(char *dst, size_t dst_size, const char *path);

// Get file extension (including the dot), empty string if none
void path_extension(char *dst, size_t dst_size, const char *path);

// Normalize a path (remove double slashes, resolve . and ..)
void path_normalize(char *dst, size_t dst_size, const char *path);

// Check if path is absolute
int path_is_absolute(const char *path);

// Expand ~ to home directory
void path_expand_home(char *dst, size_t dst_size, const char *path);

#endif
