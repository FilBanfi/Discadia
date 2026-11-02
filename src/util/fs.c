#include "fs.h"
#include "path_util.h"
#include "string_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <pwd.h>

int fs_file_exists(const char *path) {
    if (path == NULL) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISREG(st.st_mode);
}

int fs_dir_exists(const char *path) {
    if (path == NULL) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

int fs_mkdir_p(const char *path, int mode) {
    if (path == NULL || *path == '\0') return -1;
    
    char tmp[2048];
    str_safe_copy(tmp, path, sizeof(tmp));
    size_t len = strlen(tmp);
    
    // Remove trailing slash
    if (len > 0 && tmp[len - 1] == '/') tmp[len - 1] = '\0';
    
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
    return 0;
}

char *fs_read_file(const char *path, size_t *out_size) {
    if (path == NULL) return NULL;
    
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    
    char *buf = malloc((size_t)size + 1);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }
    
    size_t read = fread(buf, 1, (size_t)size, f);
    fclose(f);
    
    if (read != (size_t)size) {
        free(buf);
        return NULL;
    }
    
    buf[size] = '\0';
    if (out_size) *out_size = (size_t)size;
    return buf;
}

int fs_write_file(const char *path, const char *data, size_t size) {
    if (path == NULL) return -1;
    
    FILE *f = fopen(path, "wb");
    if (f == NULL) return -1;
    
    if (data != NULL && size > 0) {
        size_t written = fwrite(data, 1, size, f);
        if (written != size) {
            fclose(f);
            return -1;
        }
    }
    
    fclose(f);
    return 0;
}

long fs_file_size(const char *path) {
    if (path == NULL) return -1;
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long)st.st_size;
}

int fs_list_dir(const char *path, fs_dir_callback cb, void *userdata) {
    if (path == NULL || cb == NULL) return -1;
    
    DIR *d = opendir(path);
    if (d == NULL) return -1;
    
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char full_path[2048];
        path_join(full_path, sizeof(full_path), path, entry->d_name);
        
        int is_dir = 0;
        struct stat st;
        if (stat(full_path, &st) == 0) {
            is_dir = S_ISDIR(st.st_mode);
        } else {
#ifdef _DIRENT_HAVE_D_TYPE
            is_dir = (entry->d_type == DT_DIR);
#endif
        }
        
        if (cb(entry->d_name, full_path, is_dir, userdata) != 0) {
            break;
        }
    }
    
    closedir(d);
    return 0;
}

int fs_remove_file(const char *path) {
    if (path == NULL) return -1;
    return remove(path);
}

static void get_user_dir(char *dst, size_t dst_size, const char *env_var, const char *fallback_subdir) {
    const char *val = getenv(env_var);
    if (val != NULL && *val != '\0') {
        char sub[512];
        str_format(sub, sizeof(sub), "%s/discadia", fallback_subdir);
        path_join(dst, dst_size, val, sub);
        return;
    }
    
    const char *home = getenv("HOME");
    if (home == NULL) {
        struct passwd *pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/tmp";
    }
    
    path_join(dst, dst_size, home, fallback_subdir);
}

void fs_get_config_dir(char *dst, size_t dst_size) {
    get_user_dir(dst, dst_size, "XDG_CONFIG_HOME", ".config/discadia");
}

void fs_get_cache_dir(char *dst, size_t dst_size) {
    get_user_dir(dst, dst_size, "XDG_CACHE_HOME", ".cache/discadia");
}

void fs_get_saves_dir(char *dst, size_t dst_size) {
    get_user_dir(dst, dst_size, "XDG_DATA_HOME", ".local/share/discadia/saves");
}

void fs_get_discadia_root(char *dst, size_t dst_size) {
    /* ~/.local/share/discadia (or $XDG_DATA_HOME/discadia): parent of
     * the games/, engines/ and themes/ trees installed by the store. */
    get_user_dir(dst, dst_size, "XDG_DATA_HOME", ".local/share/discadia");
}

void fs_get_games_dir(char *dst, size_t dst_size) {
    get_user_dir(dst, dst_size, "XDG_DATA_HOME", ".local/share/discadia/games");
}

int fs_write_file_atomic(const char *path, const char *data, size_t size) {
    if (path == NULL || data == NULL) return -1;

    char tmp[2048];
    char bak[2048];
    str_format(tmp, sizeof(tmp), "%s.tmp", path);
    str_format(bak, sizeof(bak), "%s.bak", path);

    FILE *f = fopen(tmp, "wb");
    if (f == NULL) return -1;

    if (fwrite(data, 1, size, f) != size) {
        fclose(f);
        remove(tmp);
        return -1;
    }
    if (fflush(f) != 0 || fsync(fileno(f)) != 0) {
        fclose(f);
        remove(tmp);
        return -1;
    }
    fclose(f);

    // Keep a backup of the previous version (best effort).
    if (fs_file_exists(path)) {
        size_t old_size = 0;
        char *old = fs_read_file(path, &old_size);
        if (old != NULL) {
            fs_write_file(bak, old, old_size);
            free(old);
        }
    }

    if (rename(tmp, path) != 0) {
        remove(tmp);
        return -1;
    }
    return 0;
}

static int rmtree_cb(const char *name, const char *full_path, int is_dir, void *userdata) {
    (void)name;
    (void)userdata;
    if (is_dir) {
        return fs_rmtree(full_path);
    }
    return remove(full_path);
}

int fs_rmtree(const char *path) {
    if (path == NULL) return -1;
    if (!fs_dir_exists(path)) return remove(path);

    if (fs_list_dir(path, rmtree_cb, NULL) != 0) return -1;
    return rmdir(path);
}

long long fs_free_space_bytes(const char *path) {
    if (path == NULL) return -1;
    struct statvfs st;
    if (statvfs(path, &st) != 0) return -1;
    return (long long)st.f_bavail * (long long)st.f_frsize;
}
