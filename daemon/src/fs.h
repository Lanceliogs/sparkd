#ifndef SPARK_FS_H
#define SPARK_FS_H

#include <stddef.h>

/* Existence checks — return 1 if exists, 0 otherwise */
int spark_fs_dir_exists(const char *path);
int spark_fs_file_exists(const char *path);

/* Directory listing via callback */
typedef void (*spark_fs_list_cb)(const char *name, int is_dir, void *ctx);
int spark_fs_list_dir(const char *path, spark_fs_list_cb cb, void *ctx);

/* Directory/file operations */
int spark_fs_mkdir_p(const char *path);
int spark_fs_copy(const char *src, const char *dst);
int spark_fs_remove(const char *path);

/* Path resolution (I/O-backed) */
int spark_fs_exe_dir(char *buf, size_t size);
int spark_fs_home(char *buf, size_t size);
int spark_fs_realpath(char *buf, size_t size, const char *path);

/* Path manipulation (pure string, no I/O) */
int  spark_fs_path_join(char *buf, size_t size, const char *base, const char *child);
int  spark_fs_path_parent(char *buf, size_t size, const char *path);
void spark_fs_path_normalize(char *path);

/* Drive/mount enumeration */
typedef void (*spark_fs_drive_cb)(const char *label, const char *path, void *ctx);
int spark_fs_list_drives(spark_fs_drive_cb cb, void *ctx);

#endif
