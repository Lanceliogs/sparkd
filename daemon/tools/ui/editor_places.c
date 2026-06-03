#include "editor_places.h"
#include "env.h"
#include "fs.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void s_add_place(editor_roots_t *roots, const char *label, const char *path)
{
    if (roots->place_count >= EDITOR_PLACES_MAX) return;
    if (!path || !spark_fs_dir_exists(path)) return;

    editor_place_t *p = &roots->places[roots->place_count++];
    strncpy(p->label, label, EDITOR_PLACE_LABEL_MAX - 1);
    strncpy(p->path, path, EDITOR_PLACE_PATH_MAX - 1);
}

static void s_parse_project_roots(editor_roots_t *roots)
{
    const char *val = spark_env_get("SPARK_PROJECT_ROOTS");
    if (!val || val[0] == '\0') return;

    char buf[4096];
    strncpy(buf, val, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *saveptr = NULL;
    char *token = strtok_r(buf, ";", &saveptr);
    while (token)
    {
        while (*token == ' ') token++;
        size_t tlen = strlen(token);
        while (tlen > 0 && token[tlen - 1] == ' ') token[--tlen] = '\0';

        if (tlen > 0 && roots->project_count < EDITOR_PLACES_MAX)
        {
            char *at = strchr(token, '@');
            if (at && at != token && at[1] != '\0')
            {
                *at = '\0';
                const char *label = token;
                const char *path = at + 1;

                if (spark_fs_dir_exists(path))
                {
                    editor_place_t *p = &roots->projects[roots->project_count++];
                    strncpy(p->label, label, EDITOR_PLACE_LABEL_MAX - 1);
                    strncpy(p->path, path, EDITOR_PLACE_PATH_MAX - 1);
                }
            }
        }
        token = strtok_r(NULL, ";", &saveptr);
    }
}


void editor_get_roots(editor_roots_t *roots)
{
    memset(roots, 0, sizeof(*roots));

    s_parse_project_roots(roots);

    char home[EDITOR_PLACE_PATH_MAX] = {0};
    spark_fs_home(home, sizeof(home));

    if (home[0])
    {
        s_add_place(roots, "Home", home);

        char buf[EDITOR_PLACE_PATH_MAX];
        spark_fs_path_join(buf, sizeof(buf), home, "Downloads");
        s_add_place(roots, "Downloads", buf);

        spark_fs_path_join(buf, sizeof(buf), home, "Documents");
        s_add_place(roots, "Documents", buf);

#ifdef _WIN32
        spark_fs_path_join(buf, sizeof(buf), home, "Desktop");
        s_add_place(roots, "Desktop", buf);
#endif
    }

#ifndef _WIN32
    s_add_place(roots, "/", "/");
#endif

    spark_fs_drives_t drives;
    if (spark_fs_list_drives(&drives) == 0)
    {
        for (int i = 0; i < drives.count && roots->drive_count < EDITOR_PLACES_MAX; i++)
        {
            editor_place_t *d = &roots->drives[roots->drive_count++];
            strncpy(d->label, drives.entries[i].label, EDITOR_PLACE_LABEL_MAX - 1);
            strncpy(d->path, drives.entries[i].path, EDITOR_PLACE_PATH_MAX - 1);
        }
        spark_fs_list_drives_free(&drives);
    }
}
