/*
 * consts.h - Global defines across the project
 *
 * All the size limits of the preallocated memory is defined
 * here. You can increase the limits if needed, but I should have
 * taken some good margin already. 
 */

#ifndef SPARK_CONSTS_H
#define SPARK_CONSTS_H

#define SPARKD_VERSION "0.1.0"

#define SPARK_PROJECT_PATH_STRLEN 1024
#define SPARK_SERIAL_PORT_STRLEN 256

#define SPARK_MIDI_PORT_STRLEN 256
#define SPARK_MIDI_MAX_DEVICES 8
#define SPARK_MIDI_BUFFER_SIZE 64 /* Increase if you receive a truckload of midi events per tick */

#define SPARK_DMX_UNIVERSE_SIZE 512 /* We do DMX512 - do not increase */

#define SPARK_SCENES_MAX_COUNT 2048 /* 16 channels * 128 notes - do not increase */
#define SPARK_ACTIVE_SCENES_MAX 32

#define SPARK_FIXTURES_MAX 128
#define SPARK_CHANNEL_ARENA_SIZE 512

#define SPARK_MAX_ID_SIZE 64
#define SPARK_MAX_NAME_SIZE 64
#define SPARK_MAX_COMMENT_SIZE 128

#define SPARK_SCENE_DEFS_MAX 256
#define SPARK_SCENE_VALUE_DEF_ARENA_SIZE 2048
#define SPARK_SCENE_STEP_DEF_ARENA_SIZE 512
#define SPARK_SCENE_VALUE_ARENA_SIZE 4096
#define SPARK_SCENE_STEP_ARENA_SIZE 256

#endif