#include "editor_places.h"
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>

static void s_add_place(editor_roots_t *roots, const char *label, const char *path)
{
    if (roots->place_count >= EDITOR_PLACES_MAX) return;
    if (!path || path[0] == '\0') return;

    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attr)) return;
    if (!(attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return;

    editor_place_t *p = &roots->places[roots->place_count++];
    strncpy(p->label, label, EDITOR_PLACE_LABEL_MAX - 1);
    strncpy(p->path, path, EDITOR_PLACE_PATH_MAX - 1);
}

void editor_get_roots(editor_roots_t *roots)
{
    memset(roots, 0, sizeof(*roots));

    char home[EDITOR_PLACE_PATH_MAX] = {0};
    const char *userprofile = getenv("USERPROFILE");
    if (userprofile)
        strncpy(home, userprofile, sizeof(home) - 1);

    if (home[0])
    {
        s_add_place(roots, "Home", home);

        char buf[EDITOR_PLACE_PATH_MAX + 16];
        snprintf(buf, sizeof(buf), "%s\\Desktop", home);
        s_add_place(roots, "Desktop", buf);

        snprintf(buf, sizeof(buf), "%s\\Downloads", home);
        s_add_place(roots, "Downloads", buf);

        snprintf(buf, sizeof(buf), "%s\\Documents", home);
        s_add_place(roots, "Documents", buf);
    }

    DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; i++)
    {
        if (!(mask & (1u << i))) continue;
        if (roots->drive_count >= EDITOR_PLACES_MAX) break;

        editor_place_t *d = &roots->drives[roots->drive_count++];
        d->label[0] = (char)('A' + i);
        d->label[1] = ':';
        d->label[2] = '\0';
        snprintf(d->path, EDITOR_PLACE_PATH_MAX, "%c:/", 'A' + i);
    }
}

#else

#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

static int s_dir_exists(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

static void s_add_place(editor_roots_t *roots, const char *label, const char *path)
{
    if (roots->place_count >= EDITOR_PLACES_MAX) return;
    if (!path || !s_dir_exists(path)) return;

    editor_place_t *p = &roots->places[roots->place_count++];
    strncpy(p->label, label, EDITOR_PLACE_LABEL_MAX - 1);
    strncpy(p->path, path, EDITOR_PLACE_PATH_MAX - 1);
}

static void s_add_drive(editor_roots_t *roots, const char *label, const char *path)
{
    if (roots->drive_count >= EDITOR_PLACES_MAX) return;
    if (!path || !s_dir_exists(path)) return;

    editor_place_t *d = &roots->drives[roots->drive_count++];
    strncpy(d->label, label, EDITOR_PLACE_LABEL_MAX - 1);
    strncpy(d->path, path, EDITOR_PLACE_PATH_MAX - 1);
}

static void s_scan_media(editor_roots_t *roots, const char *base)
{
    DIR *dir = opendir(base);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_name[0] == '.') continue;
        char fullpath[EDITOR_PLACE_PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", base, entry->d_name);
        if (s_dir_exists(fullpath))
            s_add_drive(roots, entry->d_name, fullpath);
    }
    closedir(dir);
}

void editor_get_roots(editor_roots_t *roots)
{
    memset(roots, 0, sizeof(*roots));

    const char *home = getenv("HOME");
    if (home && home[0])
    {
        s_add_place(roots, "Home", home);

        char buf[EDITOR_PLACE_PATH_MAX];
        snprintf(buf, sizeof(buf), "%s/Downloads", home);
        s_add_place(roots, "Downloads", buf);

        snprintf(buf, sizeof(buf), "%s/Documents", home);
        s_add_place(roots, "Documents", buf);
    }

    s_add_place(roots, "/", "/");

    /* Scan for mounted media */
    if (home)
    {
        const char *user = strrchr(home, '/');
        if (user)
        {
            char media[EDITOR_PLACE_PATH_MAX];
            snprintf(media, sizeof(media), "/media%s", user);
            s_scan_media(roots, media);
        }
    }
    s_scan_media(roots, "/mnt");
}

#endif
