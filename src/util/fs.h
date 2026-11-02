#ifndef DISCADIA_FS_H
#define DISCADIA_FS_H

#include <stddef.h>

// Check if a file exists
int fs_file_exists(const char *path);

// Check if a directory exists
int fs_dir_exists(const char *path);

// Create directory (and parents) recursively. Returns 0 on success.
int fs_mkdir_p(const char *path, int mode);

// Read entire file into a malloc'd buffer. Caller must free.
// Returns NULL on failure. If out_size is non-NULL, sets it to file size.
char *fs_read_file(const char *path, size_t *out_size);

// Write buffer to file, creating/truncating. Returns 0 on success.
int fs_write_file(const char *path, const char *data, size_t size);

// Get file size in bytes, or -1 on error
long fs_file_size(const char *path);

// List directory entries (non-recursive). Calls callback for each entry.
// callback returns 0 to continue, non-zero to stop.
typedef int (*fs_dir_callback)(const char *name, const char *full_path, int is_dir, void *userdata);
int fs_list_dir(const char *path, fs_dir_callback cb, void *userdata);

// Delete a file. Returns 0 on success.
int fs_remove_file(const char *path);

// Get user's config directory (~/.config/discadia)
void fs_get_config_dir(char *dst, size_t dst_size);

// Get user's cache directory (~/.cache/discadia)
void fs_get_cache_dir(char *dst, size_t dst_size);

// Get user's saves directory (~/.local/share/discadia/saves)
void fs_get_saves_dir(char *dst, size_t dst_size);

// Get the Discadia user data root (~/.local/share/discadia). Parent of
// games/, engines/ and themes/ installed by the store.
void fs_get_discadia_root(char *dst, size_t dst_size);

// Get user's installed-games directory (~/.local/share/discadia/games)
void fs_get_games_dir(char *dst, size_t dst_size);

// Atomic write: writes to path.tmp then rename()s over the target, and
// keeps a .bak copy of the previous content. Never leaves a half-written
// file behind even on power loss/crash. Returns 0 on success.
int fs_write_file_atomic(const char *path, const char *data, size_t size);

// Recursively delete a directory tree. Returns 0 on success.
int fs_rmtree(const char *path);

// Free space in bytes on the filesystem containing path, or -1 on error.
long long fs_free_space_bytes(const char *path);

#endif
