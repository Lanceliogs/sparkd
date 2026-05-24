/*
 * consts.h - Global defines across the project
 *
 * All the size limits of the preallocated memory is defined
 * here. You can increase the limits if needed, but I should have
 * taken some good margin already.
 *
 * Every limit is guarded with #ifndef so you can override them
 * at compile time via -DSPARK_XXX=value without touching this file.
 */

#ifndef SPARK_CONSTS_H
#define SPARK_CONSTS_H

#define SPARKD_VERSION "0.1.0"

#ifndef SPARK_PROJECT_PATH_STRLEN
#define SPARK_PROJECT_PATH_STRLEN 1024
#endif

#ifndef SPARK_SERIAL_PORT_STRLEN
#define SPARK_SERIAL_PORT_STRLEN 256
#endif

#ifndef SPARK_MIDI_PORT_STRLEN
#define SPARK_MIDI_PORT_STRLEN 256
#endif

#ifndef SPARK_MIDI_MAX_DEVICES
#define SPARK_MIDI_MAX_DEVICES 8
#endif

#ifndef SPARK_MIDI_BUFFER_SIZE
#define SPARK_MIDI_BUFFER_SIZE 64
#endif

#ifndef SPARK_DMX_UNIVERSE_SIZE
#define SPARK_DMX_UNIVERSE_SIZE 512 /* We do DMX512 - do not increase */
#endif

#ifndef SPARK_SCENES_MAX_COUNT
#define SPARK_SCENES_MAX_COUNT 2048 /* 16 channels * 128 notes - do not increase */
#endif

#ifndef SPARK_ACTIVE_SCENES_MAX
#define SPARK_ACTIVE_SCENES_MAX 32
#endif

#ifndef SPARK_FIXTURES_MAX
#define SPARK_FIXTURES_MAX 128
#endif

#ifndef SPARK_CHANNEL_ARENA_SIZE
#define SPARK_CHANNEL_ARENA_SIZE 512
#endif

#ifndef SPARK_MAX_ID_SIZE
#define SPARK_MAX_ID_SIZE 64
#endif

#ifndef SPARK_MAX_NAME_SIZE
#define SPARK_MAX_NAME_SIZE 64
#endif

#ifndef SPARK_MAX_COMMENT_SIZE
#define SPARK_MAX_COMMENT_SIZE 128
#endif

#ifndef SPARK_SCENE_DEFS_MAX
#define SPARK_SCENE_DEFS_MAX 128
#endif

#ifndef SPARK_SCENE_VALUE_DEF_ARENA_SIZE
#define SPARK_SCENE_VALUE_DEF_ARENA_SIZE 2048
#endif

#ifndef SPARK_SCENE_STEP_DEF_ARENA_SIZE
#define SPARK_SCENE_STEP_DEF_ARENA_SIZE 512
#endif

#ifndef SPARK_SCENE_VALUE_ARENA_SIZE
#define SPARK_SCENE_VALUE_ARENA_SIZE 4096
#endif

#ifndef SPARK_SCENE_STEP_ARENA_SIZE
#define SPARK_SCENE_STEP_ARENA_SIZE 256
#endif

/* Editor limits */

#ifndef SPARK_EDITOR_MAX_FIXTURES
#define SPARK_EDITOR_MAX_FIXTURES 128
#endif

#ifndef SPARK_EDITOR_MAX_CHANNELS
#define SPARK_EDITOR_MAX_CHANNELS 64
#endif

#ifndef SPARK_EDITOR_MAX_BANK_FIXTURES
#define SPARK_EDITOR_MAX_BANK_FIXTURES 128
#endif

#ifndef SPARK_EDITOR_PATH_MAX
#define SPARK_EDITOR_PATH_MAX 1024
#endif

#ifndef SPARK_EDITOR_MAX_RAW_SECTIONS
#define SPARK_EDITOR_MAX_RAW_SECTIONS 16
#endif

#ifndef SPARK_EDITOR_MAX_SCENES
#define SPARK_EDITOR_MAX_SCENES 128
#endif

#ifndef SPARK_EDITOR_MAX_SCENE_VALUES
#define SPARK_EDITOR_MAX_SCENE_VALUES 64
#endif

#ifndef SPARK_EDITOR_MAX_SCENE_STEPS
#define SPARK_EDITOR_MAX_SCENE_STEPS 32
#endif

#ifndef SPARK_EDITOR_MAX_BANKS
#define SPARK_EDITOR_MAX_BANKS 32
#endif

#endif
