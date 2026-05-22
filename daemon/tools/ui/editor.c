#include "editor.h"
#include "log.h"
#include "env.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

#define PATH_LIST_SEP ';'

static int s_is_yaml_file(const char *name)
{
    size_t len = strlen(name);
    if (len >= 5 && strcmp(name + len - 5, ".yaml") == 0) return 1;
    if (len >= 4 && strcmp(name + len - 4, ".yml") == 0) return 1;
    return 0;
}

/* ---- Project lifecycle ---- */

int editor_open_project(editor_state_t *state, const char *path)
{
    editor_close_project(state);

    if (editor_yaml_parse_project(path, &state->project) != 0)
        return -1;

    spark_log_info("editor: opened project '%s' (%d fixtures)",
                   path, state->project.fixture_count);
    return 0;
}

void editor_close_project(editor_state_t *state)
{
    free(state->project.raw_buf);
    memset(&state->project, 0, sizeof(state->project));
}

int editor_save_project(editor_state_t *state)
{
    if (!state->project.loaded)
    {
        spark_log_error("editor: no project open");
        return -1;
    }
    if (editor_yaml_emit_project(state->project.path, &state->project) != 0)
        return -1;
    state->project.dirty = false;
    spark_log_info("editor: saved project '%s'", state->project.path);
    return 0;
}

int editor_save_project_as(editor_state_t *state, const char *path)
{
    if (!state->project.loaded)
    {
        spark_log_error("editor: no project open");
        return -1;
    }
    strncpy(state->project.path, path, EDITOR_PATH_MAX - 1);
    return editor_save_project(state);
}

/* ---- Project fixtures CRUD ---- */

int editor_fixture_add(editor_state_t *state, const editor_fixture_t *fixture)
{
    if (state->project.fixture_count >= EDITOR_MAX_FIXTURES)
        return -1;
    state->project.fixtures[state->project.fixture_count++] = *fixture;
    state->project.dirty = true;
    return 0;
}

int editor_fixture_update(editor_state_t *state, int index, const editor_fixture_t *fixture)
{
    if (index < 0 || index >= state->project.fixture_count)
        return -1;
    state->project.fixtures[index] = *fixture;
    state->project.dirty = true;
    return 0;
}

int editor_fixture_remove(editor_state_t *state, int index)
{
    if (index < 0 || index >= state->project.fixture_count)
        return -1;
    int remaining = state->project.fixture_count - index - 1;
    if (remaining > 0)
        memmove(&state->project.fixtures[index],
                &state->project.fixtures[index + 1],
                remaining * sizeof(editor_fixture_t));
    state->project.fixture_count--;
    state->project.dirty = true;
    return 0;
}

/* ---- Bank loading ---- */

static int s_load_bank_file(editor_state_t *state, const char *dir_path, const char *filename)
{
    if (!s_is_yaml_file(filename))
        return 0;

    if (state->bank_count >= EDITOR_MAX_BANKS)
        return 0;

    char filepath[EDITOR_PATH_MAX];
    snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, filename);

    editor_bank_t *bank = &state->banks[state->bank_count];
    if (editor_yaml_parse_bank(filepath, bank) != 0)
    {
        spark_log_warn("editor: failed to parse bank '%s'", filepath);
        return 0;
    }

    if (bank->id[0] == '\0')
    {
        spark_log_warn("editor: bank '%s' has no id, skipped", filepath);
        return 0;
    }

    for (uint16_t i = 0; i < state->bank_count; i++)
    {
        if (strcmp(state->banks[i].id, bank->id) == 0)
        {
            spark_log_warn("editor: duplicate bank id '%s' in '%s', skipped",
                           bank->id, filepath);
            return 0;
        }
    }

    state->bank_count++;
    spark_log_info("editor: loaded bank '%s' (%d fixtures)",
                   bank->id, bank->fixture_count);
    return 0;
}

#ifdef _WIN32

static int s_scan_bank_dir(editor_state_t *state, const char *dir_path)
{
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s/*", dir_path);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return -1;

    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        s_load_bank_file(state, dir_path, fd.cFileName);
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    return 0;
}

#else

static int s_scan_bank_dir(editor_state_t *state, const char *dir_path)
{
    DIR *dir = opendir(dir_path);
    if (!dir) return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
        s_load_bank_file(state, dir_path, entry->d_name);

    closedir(dir);
    return 0;
}

#endif

int editor_load_banks(editor_state_t *state, const char *search_paths)
{
    state->bank_count = 0;

    if (!search_paths || search_paths[0] == '\0')
    {
        char default_path[1024];
#ifdef _WIN32
        const char *home = getenv("USERPROFILE");
#else
        const char *home = getenv("HOME");
#endif
        if (!home) return 0;
        snprintf(default_path, sizeof(default_path), "%s/.spark/fixtures", home);
        s_scan_bank_dir(state, default_path);
        return 0;
    }

    char buf[4096];
    strncpy(buf, search_paths, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *saveptr = NULL;
    char *token = strtok_r(buf, (char[]){PATH_LIST_SEP, '\0'}, &saveptr);
    while (token)
    {
        while (*token == ' ') token++;
        size_t tlen = strlen(token);
        while (tlen > 0 && token[tlen - 1] == ' ') token[--tlen] = '\0';
        if (tlen > 0)
            s_scan_bank_dir(state, token);
        token = strtok_r(NULL, (char[]){PATH_LIST_SEP, '\0'}, &saveptr);
    }

    return 0;
}

/* ---- Bank fixture CRUD ---- */

int editor_bank_fixture_add(editor_state_t *state, int bank_index,
                            const editor_bank_fixture_t *fixture)
{
    if (bank_index < 0 || bank_index >= state->bank_count)
        return -1;
    editor_bank_t *bank = &state->banks[bank_index];
    if (bank->fixture_count >= EDITOR_MAX_BANK_FIXTURES)
        return -1;
    bank->fixtures[bank->fixture_count++] = *fixture;
    bank->dirty = true;
    return 0;
}

int editor_bank_fixture_update(editor_state_t *state, int bank_index,
                               int fixture_index, const editor_bank_fixture_t *fixture)
{
    if (bank_index < 0 || bank_index >= state->bank_count)
        return -1;
    editor_bank_t *bank = &state->banks[bank_index];
    if (fixture_index < 0 || fixture_index >= bank->fixture_count)
        return -1;
    bank->fixtures[fixture_index] = *fixture;
    bank->dirty = true;
    return 0;
}

int editor_bank_fixture_remove(editor_state_t *state, int bank_index, int fixture_index)
{
    if (bank_index < 0 || bank_index >= state->bank_count)
        return -1;
    editor_bank_t *bank = &state->banks[bank_index];
    if (fixture_index < 0 || fixture_index >= bank->fixture_count)
        return -1;

    int remaining = bank->fixture_count - fixture_index - 1;
    if (remaining > 0)
        memmove(&bank->fixtures[fixture_index],
                &bank->fixtures[fixture_index + 1],
                remaining * sizeof(editor_bank_fixture_t));
    bank->fixture_count--;
    bank->dirty = true;
    return 0;
}

int editor_save_bank(editor_state_t *state, int bank_index)
{
    if (bank_index < 0 || bank_index >= state->bank_count)
        return -1;
    editor_bank_t *bank = &state->banks[bank_index];
    if (editor_yaml_emit_bank(bank->path, bank) != 0)
        return -1;
    bank->dirty = false;
    spark_log_info("editor: saved bank '%s'", bank->id);
    return 0;
}
