#ifndef SPARK_FS_H
#define SPARK_FS_H

#include <stddef.h>

/* Return 1 if path exists and is a directory, 0 otherwise. */
int spark_fs_dir_exists(const char *path);

/* Return 1 if path exists and is a regular file, 0 otherwise. */
int spark_fs_file_exists(const char *path);

/* Iterate directory entries, calling cb(name, is_dir, ctx) for each.
 * Skips "." and "..". Returns 0 on success, -1 if path cannot be opened. */
typedef void (*spark_fs_iter_callback)(const char *name, int is_dir, void *ctx);
int spark_fs_iter_dir(const char *path, spark_fs_iter_callback cb, void *ctx);

/* Directory entry returned by spark_fs_list. */
typedef struct {
    char name[256];
    int is_dir;
} spark_fs_entry_t;

/* Result of spark_fs_list. Must be freed with spark_fs_list_free. */
typedef struct {
    spark_fs_entry_t *entries;
    int count;
} spark_fs_listing_t;

/* List all entries in a directory. Skips "." and "..".
 * Populates out->entries (heap-allocated) and out->count.
 * Returns 0 on success, -1 if path cannot be opened. */
int spark_fs_list(const char *path, spark_fs_listing_t *out);

/* Free a listing returned by spark_fs_list. */
void spark_fs_list_free(spark_fs_listing_t *out);

/* Create directory and all parent directories. Returns 0 on success. */
int spark_fs_mkdir_p(const char *path);

/* Copy file from src to dst. Returns 0 on success. */
int spark_fs_copy(const char *src, const char *dst);

/* Remove a file. Returns 0 on success. */
int spark_fs_remove(const char *path);

/* Write the directory containing the running executable into buf.
 * Returns 0 on success, -1 on failure. */
int spark_fs_exe_dir(char *buf, size_t size);

/* Write the user's home directory into buf.
 * Uses USERPROFILE on Windows, HOME on Unix.
 * Returns 0 on success, -1 on failure. */
int spark_fs_home(char *buf, size_t size);

/* Resolve path to an absolute canonical path.
 * Returns 0 on success, -1 if path does not exist or cannot be resolved. */
int spark_fs_realpath(char *buf, size_t size, const char *path);

/* Join base and child with a path separator. Deduplicates slashes.
 * Returns 0 on success, -1 if result would overflow buf. */
int spark_fs_path_join(char *buf, size_t size, const char *base, const char *child);

/* Write the parent directory of path into buf.
 * Returns 0 on success, -1 if path is already a root. */
int spark_fs_path_parent(char *buf, size_t size, const char *path);

/* Normalize path in-place: replace all backslashes with forward slashes. */
void spark_fs_path_normalize(char *path);

/* Enumerate drives (Windows) or mounted volumes (Unix).
 * Calls callback(label, path, ctx) for each. Returns 0 on success. */
typedef void (*spark_fs_drive_callback)(const char *label, const char *path, void *ctx);
int spark_fs_iter_drives(spark_fs_drive_callback callback, void *ctx);

/* Drive entry returned by spark_fs_list_drives. */
typedef struct {
    char label[32];
    char path[260];
} spark_fs_drive_t;

/* Result of spark_fs_list_drives. Must be freed with spark_fs_list_drives_free. */
typedef struct {
    spark_fs_drive_t *entries;
    int count;
} spark_fs_drives_t;

/* List all drives/mounts. Populates out->entries (heap-allocated) and out->count.
 * Returns 0 on success. */
int spark_fs_list_drives(spark_fs_drives_t *out);

/* Free a drives listing returned by spark_fs_list_drives. */
void spark_fs_list_drives_free(spark_fs_drives_t *out);

#endif
