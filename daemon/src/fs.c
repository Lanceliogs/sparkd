#include "fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#endif

/* ------------------------------------------------------------------ */
/* Existence checks                                                    */
/* ------------------------------------------------------------------ */

#ifdef _WIN32

int spark_fs_dir_exists(const char *path)
{
    if (!path || path[0] == '\0') return 0;
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attr)) return 0;
    return (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
}

int spark_fs_file_exists(const char *path)
{
    if (!path || path[0] == '\0') return 0;
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attr)) return 0;
    return (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 0 : 1;
}

#else

int spark_fs_dir_exists(const char *path)
{
    if (!path || path[0] == '\0') return 0;
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
}

int spark_fs_file_exists(const char *path)
{
    if (!path || path[0] == '\0') return 0;
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode)) ? 1 : 0;
}

#endif

/* ------------------------------------------------------------------ */
/* Directory listing                                                   */
/* ------------------------------------------------------------------ */

#ifdef _WIN32

int spark_fs_list_dir(const char *path, spark_fs_list_cb cb, void *ctx)
{
    if (!path || !cb) return -1;

    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*", path);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return -1;

    do {
        if (fd.cFileName[0] == '.' &&
            (fd.cFileName[1] == '\0' ||
             (fd.cFileName[1] == '.' && fd.cFileName[2] == '\0')))
            continue;
        int is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
        cb(fd.cFileName, is_dir, ctx);
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    return 0;
}

#else

int spark_fs_list_dir(const char *path, spark_fs_list_cb cb, void *ctx)
{
    if (!path || !cb) return -1;

    DIR *dir = opendir(path);
    if (!dir) return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' ||
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
            continue;

        int is_dir = 0;
#ifdef _DIRENT_HAVE_D_TYPE
        if (entry->d_type == DT_DIR)
            is_dir = 1;
        else if (entry->d_type == DT_UNKNOWN)
#endif
        {
            char full[1024];
            snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);
            struct stat st;
            if (stat(full, &st) == 0 && S_ISDIR(st.st_mode))
                is_dir = 1;
        }
        cb(entry->d_name, is_dir, ctx);
    }
    closedir(dir);
    return 0;
}

#endif

/* ------------------------------------------------------------------ */
/* mkdir -p                                                            */
/* ------------------------------------------------------------------ */

int spark_fs_mkdir_p(const char *path)
{
    if (!path || path[0] == '\0') return -1;

    char tmp[1024];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    spark_fs_path_normalize(tmp);

    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/')
        tmp[--len] = '\0';

    /* Start after drive letter or root slash */
    char *p = tmp;
    if (len >= 2 && tmp[1] == ':') p += 2;
    if (*p == '/') p++;

    for (; *p; p++)
    {
        if (*p == '/')
        {
            *p = '\0';
#ifdef _WIN32
            _mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            *p = '/';
        }
    }

#ifdef _WIN32
    _mkdir(tmp);
#else
    mkdir(tmp, 0755);
#endif

    return spark_fs_dir_exists(tmp) ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* File copy                                                           */
/* ------------------------------------------------------------------ */

int spark_fs_copy(const char *src, const char *dst)
{
    if (!src || !dst) return -1;

    FILE *in = fopen(src, "rb");
    if (!in) return -1;

    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return -1; }

    char buf[8192];
    size_t n;
    int rc = 0;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
    {
        if (fwrite(buf, 1, n, out) != n) { rc = -1; break; }
    }

    fclose(in);
    fclose(out);
    return rc;
}

/* ------------------------------------------------------------------ */
/* File remove                                                         */
/* ------------------------------------------------------------------ */

int spark_fs_remove(const char *path)
{
    if (!path) return -1;
    return remove(path) == 0 ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Executable directory                                                */
/* ------------------------------------------------------------------ */

#ifdef _WIN32

int spark_fs_exe_dir(char *buf, size_t size)
{
    if (!buf || size == 0) return -1;
    char tmp[1024];
    DWORD len = GetModuleFileNameA(NULL, tmp, sizeof(tmp));
    if (len == 0 || len >= sizeof(tmp)) return -1;

    /* Strip filename — find last separator */
    char *sep = strrchr(tmp, '\\');
    if (!sep) sep = strrchr(tmp, '/');
    if (sep) *sep = '\0';
    else tmp[0] = '\0';

    if (strlen(tmp) >= size) return -1;
    strncpy(buf, tmp, size - 1);
    buf[size - 1] = '\0';
    spark_fs_path_normalize(buf);
    return 0;
}

#else

int spark_fs_exe_dir(char *buf, size_t size)
{
    if (!buf || size == 0) return -1;
    char tmp[1024];
    ssize_t len = readlink("/proc/self/exe", tmp, sizeof(tmp) - 1);
    if (len <= 0) return -1;
    tmp[len] = '\0';

    char *sep = strrchr(tmp, '/');
    if (sep) *sep = '\0';
    else tmp[0] = '\0';

    if ((size_t)strlen(tmp) >= size) return -1;
    strncpy(buf, tmp, size - 1);
    buf[size - 1] = '\0';
    return 0;
}

#endif

/* ------------------------------------------------------------------ */
/* Home directory                                                      */
/* ------------------------------------------------------------------ */

int spark_fs_home(char *buf, size_t size)
{
    if (!buf || size == 0) return -1;

#ifdef _WIN32
    const char *home = getenv("USERPROFILE");
#else
    const char *home = getenv("HOME");
#endif

    if (!home || home[0] == '\0') return -1;
    if (strlen(home) >= size) return -1;
    strncpy(buf, home, size - 1);
    buf[size - 1] = '\0';
    spark_fs_path_normalize(buf);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Realpath                                                            */
/* ------------------------------------------------------------------ */

#ifdef _WIN32

int spark_fs_realpath(char *buf, size_t size, const char *path)
{
    if (!buf || !path || size == 0) return -1;
    char tmp[1024];
    if (!_fullpath(tmp, path, sizeof(tmp))) return -1;
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (!GetFileAttributesExA(tmp, GetFileExInfoStandard, &attr)) return -1;
    if (strlen(tmp) >= size) return -1;
    strncpy(buf, tmp, size - 1);
    buf[size - 1] = '\0';
    spark_fs_path_normalize(buf);
    return 0;
}

#else

int spark_fs_realpath(char *buf, size_t size, const char *path)
{
    if (!buf || !path || size == 0) return -1;
    char tmp[PATH_MAX];
    if (!realpath(path, tmp)) return -1;
    if (strlen(tmp) >= size) return -1;
    strncpy(buf, tmp, size - 1);
    buf[size - 1] = '\0';
    return 0;
}

#endif

/* ------------------------------------------------------------------ */
/* Path join                                                           */
/* ------------------------------------------------------------------ */

int spark_fs_path_join(char *buf, size_t size, const char *base, const char *child)
{
    if (!buf || size == 0 || !base || !child) return -1;

    size_t blen = strlen(base);
    int has_sep = (blen > 0 && (base[blen - 1] == '/' || base[blen - 1] == '\\'));
    int child_sep = (child[0] == '/' || child[0] == '\\');

    int n;
    if (has_sep && child_sep)
        n = snprintf(buf, size, "%s%s", base, child + 1);
    else if (has_sep || child_sep)
        n = snprintf(buf, size, "%s%s", base, child);
    else
        n = snprintf(buf, size, "%s/%s", base, child);

    return (n < 0 || (size_t)n >= size) ? -1 : 0;
}

/* ------------------------------------------------------------------ */
/* Path parent (dirname)                                               */
/* ------------------------------------------------------------------ */

int spark_fs_path_parent(char *buf, size_t size, const char *path)
{
    if (!buf || size == 0 || !path) return -1;

    char tmp[1024];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    size_t len = strlen(tmp);
    /* Strip trailing separators */
    while (len > 1 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\'))
        tmp[--len] = '\0';

    /* Already at root? */
    if (len == 1 && (tmp[0] == '/' || tmp[0] == '\\'))
        return -1;
    if (len == 2 && tmp[1] == ':')
        return -1;
    if (len == 3 && tmp[1] == ':' && (tmp[2] == '/' || tmp[2] == '\\'))
        return -1;

    /* Find last separator */
    char *sep = strrchr(tmp, '/');
    char *sep2 = strrchr(tmp, '\\');
    if (sep2 && (!sep || sep2 > sep)) sep = sep2;

    if (!sep)
    {
        /* No separator — drive root or relative */
        if (len >= 2 && tmp[1] == ':')
        {
            /* "C:" → "C:/" */
            if (size < 4) return -1;
            buf[0] = tmp[0]; buf[1] = ':'; buf[2] = '/'; buf[3] = '\0';
            return 0;
        }
        return -1;
    }

    /* Handle root: "/" or "C:/" */
    if (sep == tmp || (sep == tmp + 2 && tmp[1] == ':'))
    {
        size_t root_len = (size_t)(sep - tmp) + 1;
        if (root_len >= size) return -1;
        memcpy(buf, tmp, root_len);
        buf[root_len] = '\0';
        return 0;
    }

    size_t plen = (size_t)(sep - tmp);
    if (plen >= size) return -1;
    memcpy(buf, tmp, plen);
    buf[plen] = '\0';
    return 0;
}

/* ------------------------------------------------------------------ */
/* Path normalize (in-place backslash to forward slash)                */
/* ------------------------------------------------------------------ */

void spark_fs_path_normalize(char *path)
{
    if (!path) return;
    for (char *p = path; *p; p++)
        if (*p == '\\') *p = '/';
}

/* ------------------------------------------------------------------ */
/* Drive/mount enumeration                                             */
/* ------------------------------------------------------------------ */

#ifdef _WIN32

int spark_fs_list_drives(spark_fs_drive_cb cb, void *ctx)
{
    if (!cb) return -1;
    DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; i++)
    {
        if (!(mask & (1u << i))) continue;
        char label[4] = { (char)('A' + i), ':', '\0', '\0' };
        char path[4] = { (char)('A' + i), ':', '/', '\0' };
        cb(label, path, ctx);
    }
    return 0;
}

#else

static void s_scan_mounts(const char *base, spark_fs_drive_cb cb, void *ctx)
{
    DIR *dir = opendir(base);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_name[0] == '.') continue;
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", base, entry->d_name);
        if (spark_fs_dir_exists(fullpath))
            cb(entry->d_name, fullpath, ctx);
    }
    closedir(dir);
}

int spark_fs_list_drives(spark_fs_drive_cb cb, void *ctx)
{
    if (!cb) return -1;

    const char *home = getenv("HOME");
    if (home)
    {
        const char *user = strrchr(home, '/');
        if (user)
        {
            char media[1024];
            snprintf(media, sizeof(media), "/media%s", user);
            s_scan_mounts(media, cb, ctx);
        }
    }
    s_scan_mounts("/mnt", cb, ctx);
    return 0;
}

#endif
