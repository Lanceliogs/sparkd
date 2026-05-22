/*
 * editor.h - In-memory editor model for spark-ui
 *
 * Holds editable copies of project fixtures and fixture bank templates.
 * Changes are tracked via a dirty flag and can be persisted back to YAML.
 */
#ifndef SPARK_UI_EDITOR_H
#define SPARK_UI_EDITOR_H

#include "consts.h"

#include <stdbool.h>
#include <stdint.h>

#define EDITOR_MAX_FIXTURES 128
#define EDITOR_MAX_CHANNELS 64
#define EDITOR_MAX_BANK_FIXTURES 256
#define EDITOR_PATH_MAX 1024

typedef struct {
    char name[SPARK_MAX_NAME_SIZE];
    uint8_t offset;
} editor_channel_t;

typedef struct {
    char id[SPARK_MAX_ID_SIZE];
    char name[SPARK_MAX_NAME_SIZE];
    uint16_t start_address;
    uint8_t channel_count;
    editor_channel_t channels[EDITOR_MAX_CHANNELS];
    char template_ref[128];
    char copy_from[SPARK_MAX_ID_SIZE];
} editor_fixture_t;

typedef struct {
    char path[EDITOR_PATH_MAX];
    bool loaded;
    bool dirty;
    uint16_t fixture_count;
    editor_fixture_t fixtures[EDITOR_MAX_FIXTURES];
} editor_project_t;

typedef struct {
    char id[SPARK_MAX_ID_SIZE];
    char name[SPARK_MAX_NAME_SIZE];
    uint8_t channel_count;
    editor_channel_t channels[EDITOR_MAX_CHANNELS];
} editor_bank_fixture_t;

typedef struct {
    char id[SPARK_MAX_ID_SIZE];
    char path[EDITOR_PATH_MAX];
    uint16_t version;
    bool dirty;
    uint16_t fixture_count;
    editor_bank_fixture_t fixtures[EDITOR_MAX_BANK_FIXTURES];
} editor_bank_t;

#define EDITOR_MAX_BANKS 32

typedef struct {
    editor_project_t project;
    uint16_t bank_count;
    editor_bank_t banks[EDITOR_MAX_BANKS];
} editor_state_t;

/* Lifecycle */
int  editor_open_project(editor_state_t *state, const char *path);
void editor_close_project(editor_state_t *state);
int  editor_save_project(editor_state_t *state);
int  editor_save_project_as(editor_state_t *state, const char *path);

/* Project fixtures */
int  editor_fixture_add(editor_state_t *state, const editor_fixture_t *fixture);
int  editor_fixture_update(editor_state_t *state, int index, const editor_fixture_t *fixture);
int  editor_fixture_remove(editor_state_t *state, int index);

/* Banks */
int  editor_load_banks(editor_state_t *state, const char *search_paths);
int  editor_bank_fixture_add(editor_state_t *state, int bank_index, const editor_bank_fixture_t *fixture);
int  editor_bank_fixture_update(editor_state_t *state, int bank_index, int fixture_index, const editor_bank_fixture_t *fixture);
int  editor_bank_fixture_remove(editor_state_t *state, int bank_index, int fixture_index);
int  editor_save_bank(editor_state_t *state, int bank_index);

/* YAML I/O (implemented in editor_yaml.c) */
int  editor_yaml_parse_project(const char *path, editor_project_t *project);
int  editor_yaml_emit_project(const char *path, const editor_project_t *project);
int  editor_yaml_parse_bank(const char *path, editor_bank_t *bank);
int  editor_yaml_emit_bank(const char *path, const editor_bank_t *bank);

#endif
