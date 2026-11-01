#include "path_util.h"
#include "string_util.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>

void path_join(char *dst, size_t dst_size, const char *a, const char *b) {
    if (dst == NULL || dst_size == 0) return;
    dst[0] = '\0';
    
    if (a == NULL && b == NULL) return;
    if (a == NULL) { str_safe_copy(dst, b, dst_size); return; }
    if (b == NULL) { str_safe_copy(dst, a, dst_size); return; }
    
    size_t a_len = strlen(a);
    int a_has_slash = (a_len > 0 && a[a_len - 1] == '/');
    int b_has_slash = (b[0] == '/');
    
    if (a_has_slash && b_has_slash) {
        str_format(dst, dst_size, "%s%s", a, b + 1);
    } else if (!a_has_slash && !b_has_slash) {
        str_format(dst, dst_size, "%s/%s", a, b);
    } else {
        str_format(dst, dst_size, "%s%s", a, b);
    }
}

void path_dirname(char *dst, size_t dst_size, const char *path) {
    if (dst == NULL || dst_size == 0) return;
    if (path == NULL) { dst[0] = '\0'; return; }
    
    const char *last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        str_safe_copy(dst, ".", dst_size);
        return;
    }
    if (last_slash == path) {
        str_safe_copy(dst, "/", dst_size);
        return;
    }
    
    size_t len = (size_t)(last_slash - path);
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, path, len);
    dst[len] = '\0';
}

void path_basename(char *dst, size_t dst_size, const char *path) {
    if (dst == NULL || dst_size == 0) return;
    if (path == NULL) { dst[0] = '\0'; return; }
    
    const char *last_slash = strrchr(path, '/');
    str_safe_copy(dst, last_slash ? last_slash + 1 : path, dst_size);
}

void path_extension(char *dst, size_t dst_size, const char *path) {
    if (dst == NULL || dst_size == 0) return;
    dst[0] = '\0';
    if (path == NULL) return;
    
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    
    const char *dot = strrchr(base, '.');
    if (dot == NULL || dot == base) return;
    
    str_safe_copy(dst, dot, dst_size);
}

void path_normalize(char *dst, size_t dst_size, const char *path) {
    if (dst == NULL || dst_size == 0) return;
    if (path == NULL) { dst[0] = '\0'; return; }
    
    // Simple normalization: collapse multiple slashes
    char temp[2048];
    str_safe_copy(temp, path, sizeof(temp));
    
    char *src = temp;
    char *out = dst;
    char *end = dst + dst_size - 1;
    int prev_slash = 0;
    
    while (*src && out < end) {
        if (*src == '/') {
            if (!prev_slash) {
                *out++ = '/';
            }
            prev_slash = 1;
        } else {
            *out++ = *src;
            prev_slash = 0;
        }
        src++;
    }
    *out = '\0';
    
    // Remove trailing slash (unless it's just "/")
    size_t len = strlen(dst);
    if (len > 1 && dst[len - 1] == '/') {
        dst[len - 1] = '\0';
    }
}

int path_is_absolute(const char *path) {
    return path != NULL && path[0] == '/';
}

void path_expand_home(char *dst, size_t dst_size, const char *path) {
    if (dst == NULL || dst_size == 0) return;
    if (path == NULL) { dst[0] = '\0'; return; }
    
    if (path[0] != '~') {
        str_safe_copy(dst, path, dst_size);
        return;
    }
    
    const char *home = getenv("HOME");
    if (home == NULL) {
        struct passwd *pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/tmp";
    }
    
    if (path[1] == '\0' || path[1] == '/') {
        path_join(dst, dst_size, home, path + 1);
    } else {
        str_safe_copy(dst, path, dst_size);
    }
}
