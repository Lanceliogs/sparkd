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
#include <stddef.h>
#include <stdint.h>

#define EDITOR_MAX_FIXTURES      SPARK_EDITOR_MAX_FIXTURES
#define EDITOR_MAX_CHANNELS      SPARK_EDITOR_MAX_CHANNELS
#define EDITOR_MAX_BANK_FIXTURES SPARK_EDITOR_MAX_BANK_FIXTURES
#define EDITOR_PATH_MAX          SPARK_EDITOR_PATH_MAX
#define EDITOR_MAX_RAW_SECTIONS  SPARK_EDITOR_MAX_RAW_SECTIONS
#define EDITOR_MAX_SCENES        SPARK_EDITOR_MAX_SCENES
#define EDITOR_MAX_SCENE_VALUES  SPARK_EDITOR_MAX_SCENE_VALUES
#define EDITOR_MAX_SCENE_STEPS   SPARK_EDITOR_MAX_SCENE_STEPS

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
    char key[64];
    size_t start;
    size_t len;
} editor_raw_section_t;

/* Hardware config (midi + dmx sections) */
typedef struct {
    char midi_device[128];
    char midi_mode[32];
    char dmx_device[128];
    char dmx_backend[32];
    uint8_t dmx_refresh_hz;
} editor_hw_config_t;

/* Scene value: "fixture.channel" -> value */
typedef struct {
    char target[128];
    uint8_t value;
} editor_scene_value_t;

/* Scene step (for sequence type) */
typedef struct {
    uint32_t duration_ms;
    char transition[16];
    uint8_t value_count;
    editor_scene_value_t values[EDITOR_MAX_SCENE_VALUES];
} editor_scene_step_t;

/* Scene definition */
typedef struct {
    char id[SPARK_MAX_ID_SIZE];
    char name[SPARK_MAX_NAME_SIZE];
    char type[16];
    char trigger_mode[16];
    uint8_t channel;
    uint8_t note;
    bool enabled;
    bool loop;
    uint8_t value_count;
    editor_scene_value_t values[EDITOR_MAX_SCENE_VALUES];
    uint8_t step_count;
    editor_scene_step_t steps[EDITOR_MAX_SCENE_STEPS];
} editor_scene_t;

typedef struct {
    char path[EDITOR_PATH_MAX];
    bool loaded;
    bool dirty;
    uint16_t fixture_count;
    editor_fixture_t fixtures[EDITOR_MAX_FIXTURES];
    /* Hardware config */
    editor_hw_config_t hw;
    /* Scenes */
    uint16_t scene_count;
    editor_scene_t scenes[EDITOR_MAX_SCENES];
    /* Preserved raw YAML for sections we don't edit (format, app) */
    char *raw_buf;
    size_t raw_buf_len;
    uint16_t raw_section_count;
    editor_raw_section_t raw_sections[EDITOR_MAX_RAW_SECTIONS];
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

#define EDITOR_MAX_BANKS SPARK_EDITOR_MAX_BANKS

typedef struct {
    editor_project_t project;
    uint16_t bank_count;
    editor_bank_t banks[EDITOR_MAX_BANKS];
} editor_state_t;

/* Parse error detail (populated on failure) */
typedef struct {
    char message[256];
    size_t line;
    size_t column;
} editor_parse_error_t;

/* Lifecycle — editor_open_project returns -1 (fatal), 0 (clean), or N>0 (problems) */
int  editor_open_project(editor_state_t *state, const char *path, editor_parse_error_t *err);
void editor_close_project(editor_state_t *state);
int  editor_save_project(editor_state_t *state);
int  editor_save_project_as(editor_state_t *state, const char *path);

/* Project fixtures */
int  editor_fixture_add(editor_state_t *state, const editor_fixture_t *fixture);
int  editor_fixture_update(editor_state_t *state, int index, const editor_fixture_t *fixture);
int  editor_fixture_remove(editor_state_t *state, int index);
void editor_fixtures_sort(editor_state_t *state);

/* Hardware config */
int  editor_hw_update(editor_state_t *state, const editor_hw_config_t *hw);

/* Scenes */
int  editor_scene_add(editor_state_t *state, const editor_scene_t *scene);
int  editor_scene_update(editor_state_t *state, int index, const editor_scene_t *scene);
int  editor_scene_remove(editor_state_t *state, int index);

/* Banks */
int  editor_load_banks(editor_state_t *state, const char *search_paths);
int  editor_bank_fixture_add(editor_state_t *state, int bank_index, const editor_bank_fixture_t *fixture);
int  editor_bank_fixture_update(editor_state_t *state, int bank_index, int fixture_index, const editor_bank_fixture_t *fixture);
int  editor_bank_fixture_remove(editor_state_t *state, int bank_index, int fixture_index);
int  editor_save_bank(editor_state_t *state, int bank_index);

/* YAML I/O (implemented in editor_yaml.c) */
int  editor_yaml_parse_project(const char *path, editor_project_t *project, editor_parse_error_t *err);
int  editor_yaml_emit_project(const char *path, const editor_project_t *project);
int  editor_yaml_parse_bank(const char *path, editor_bank_t *bank);
int  editor_yaml_emit_bank(const char *path, const editor_bank_t *bank);

#endif
