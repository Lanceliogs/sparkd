/*
 * spark-reaper - Generate REAPER integration files from a sparkd project
 *
 * Subcommands:
 *   note-names   Generate per-channel note-name files for REAPER's MIDI editor
 */

#include "log.h"

#include <yaml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#define MAX_SCENES 2048
#define MAX_NAME 64

typedef struct {
    uint8_t channel; /* 0-indexed */
    uint8_t note;
    char name[MAX_NAME];
    bool enabled;
} scene_entry_t;

static scene_entry_t s_scenes[MAX_SCENES];
static int s_scene_count = 0;

/* ---- YAML helpers ---- */

static const char *s_scalar(const yaml_event_t *ev)
{
    return (const char *)ev->data.scalar.value;
}

static int s_next(yaml_parser_t *p, yaml_event_t *ev)
{
    if (!yaml_parser_parse(p, ev)) {
        fprintf(stderr, "error: YAML parse error at line %zu: %s\n",
            p->problem_mark.line + 1, p->problem);
        return -1;
    }
    return 0;
}

static int s_skip_value(yaml_parser_t *p)
{
    yaml_event_t ev;
    if (s_next(p, &ev) != 0) return -1;

    switch (ev.type) {
    case YAML_SCALAR_EVENT:
        yaml_event_delete(&ev);
        return 0;
    case YAML_MAPPING_START_EVENT: {
        yaml_event_delete(&ev);
        int depth = 1;
        while (depth > 0) {
            if (s_next(p, &ev) != 0) return -1;
            if (ev.type == YAML_MAPPING_START_EVENT) depth++;
            else if (ev.type == YAML_MAPPING_END_EVENT) depth--;
            yaml_event_delete(&ev);
        }
        return 0;
    }
    case YAML_SEQUENCE_START_EVENT: {
        yaml_event_delete(&ev);
        int depth = 1;
        while (depth > 0) {
            if (s_next(p, &ev) != 0) return -1;
            if (ev.type == YAML_SEQUENCE_START_EVENT) depth++;
            else if (ev.type == YAML_SEQUENCE_END_EVENT) depth--;
            yaml_event_delete(&ev);
        }
        return 0;
    }
    default:
        yaml_event_delete(&ev);
        return 0;
    }
}

/* ---- Scene trigger parser ---- */

static int s_parse_trigger(yaml_parser_t *p, scene_entry_t *entry)
{
    yaml_event_t ev;

    for (;;) {
        if (s_next(p, &ev) != 0) return -1;
        if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); return 0; }

        if (ev.type != YAML_SCALAR_EVENT) {
            yaml_event_delete(&ev);
            return -1;
        }

        const char *key = s_scalar(&ev);

        if (strcmp(key, "channel") == 0) {
            yaml_event_delete(&ev);
            if (s_next(p, &ev) != 0) return -1;
            entry->channel = (uint8_t)(atoi(s_scalar(&ev)) - 1); /* YAML is 1-indexed */
            yaml_event_delete(&ev);
        } else if (strcmp(key, "note") == 0) {
            yaml_event_delete(&ev);
            if (s_next(p, &ev) != 0) return -1;
            entry->note = (uint8_t)atoi(s_scalar(&ev));
            yaml_event_delete(&ev);
        } else {
            yaml_event_delete(&ev);
            if (s_skip_value(p) != 0) return -1;
        }
    }
}

/* ---- Single scene parser ---- */

static int s_parse_scene(yaml_parser_t *p)
{
    if (s_scene_count >= MAX_SCENES) {
        fprintf(stderr, "error: too many scenes (max %d)\n", MAX_SCENES);
        return -1;
    }

    scene_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.enabled = true;

    bool has_note = false;
    yaml_event_t ev;

    for (;;) {
        if (s_next(p, &ev) != 0) return -1;
        if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); break; }

        if (ev.type != YAML_SCALAR_EVENT) {
            yaml_event_delete(&ev);
            return -1;
        }

        const char *key = s_scalar(&ev);

        if (strcmp(key, "name") == 0) {
            yaml_event_delete(&ev);
            if (s_next(p, &ev) != 0) return -1;
            strncpy(entry.name, s_scalar(&ev), MAX_NAME - 1);
            yaml_event_delete(&ev);
        } else if (strcmp(key, "enabled") == 0) {
            yaml_event_delete(&ev);
            if (s_next(p, &ev) != 0) return -1;
            const char *v = s_scalar(&ev);
            entry.enabled = (strcmp(v, "false") != 0 && strcmp(v, "no") != 0);
            yaml_event_delete(&ev);
        } else if (strcmp(key, "trigger") == 0) {
            yaml_event_delete(&ev);
            if (s_next(p, &ev) != 0) return -1;
            if (ev.type != YAML_MAPPING_START_EVENT) {
                yaml_event_delete(&ev);
                return -1;
            }
            yaml_event_delete(&ev);
            if (s_parse_trigger(p, &entry) != 0) return -1;
            has_note = true;
        } else {
            yaml_event_delete(&ev);
            if (s_skip_value(p) != 0) return -1;
        }
    }

    if (has_note && entry.name[0] != '\0') {
        s_scenes[s_scene_count++] = entry;
    }

    return 0;
}

/* ---- Scenes sequence parser ---- */

static int s_parse_scenes(yaml_parser_t *p)
{
    yaml_event_t ev;

    for (;;) {
        if (s_next(p, &ev) != 0) return -1;
        if (ev.type == YAML_SEQUENCE_END_EVENT) { yaml_event_delete(&ev); return 0; }

        if (ev.type != YAML_MAPPING_START_EVENT) {
            yaml_event_delete(&ev);
            return -1;
        }
        yaml_event_delete(&ev);

        if (s_parse_scene(p) != 0) return -1;
    }
}

/* ---- Top-level parser (finds scenes: section) ---- */

static int s_parse_project(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "error: cannot open '%s'\n", path);
        return -1;
    }

    yaml_parser_t parser;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, f);

    yaml_event_t ev;

    /* Skip stream/document start */
    if (s_next(&parser, &ev) != 0) goto fail;
    yaml_event_delete(&ev); /* STREAM_START */
    if (s_next(&parser, &ev) != 0) goto fail;
    yaml_event_delete(&ev); /* DOCUMENT_START */
    if (s_next(&parser, &ev) != 0) goto fail;
    if (ev.type != YAML_MAPPING_START_EVENT) {
        fprintf(stderr, "error: expected top-level mapping\n");
        yaml_event_delete(&ev);
        goto fail;
    }
    yaml_event_delete(&ev);

    /* Walk top-level keys */
    for (;;) {
        if (s_next(&parser, &ev) != 0) goto fail;
        if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); break; }

        if (ev.type != YAML_SCALAR_EVENT) {
            yaml_event_delete(&ev);
            if (s_skip_value(&parser) != 0) goto fail;
            continue;
        }

        const char *key = s_scalar(&ev);

        if (strcmp(key, "scenes") == 0) {
            yaml_event_delete(&ev);
            if (s_next(&parser, &ev) != 0) goto fail;
            if (ev.type != YAML_SEQUENCE_START_EVENT) {
                yaml_event_delete(&ev);
                goto fail;
            }
            yaml_event_delete(&ev);
            if (s_parse_scenes(&parser) != 0) goto fail;
        } else {
            yaml_event_delete(&ev);
            if (s_skip_value(&parser) != 0) goto fail;
        }
    }

    yaml_parser_delete(&parser);
    fclose(f);
    return 0;

fail:
    yaml_parser_delete(&parser);
    fclose(f);
    return -1;
}

/* ---- Output ---- */

static int s_compare_by_note(const void *a, const void *b)
{
    const scene_entry_t *sa = (const scene_entry_t *)a;
    const scene_entry_t *sb = (const scene_entry_t *)b;
    return (int)sa->note - (int)sb->note;
}

static int s_write_note_names(const char *out_dir, bool include_disabled)
{
    /* Find unique channels */
    uint8_t channels[16];
    int channel_count = 0;

    for (int i = 0; i < s_scene_count; i++) {
        bool found = false;
        for (int c = 0; c < channel_count; c++) {
            if (channels[c] == s_scenes[i].channel) { found = true; break; }
        }
        if (!found) {
            if (channel_count >= 16) {
                fprintf(stderr, "error: more than 16 MIDI channels used\n");
                return -1;
            }
            channels[channel_count++] = s_scenes[i].channel;
        }
    }

    if (channel_count == 0) {
        fprintf(stderr, "warning: no scenes with triggers found\n");
        return 0;
    }

    /* Sort scenes by note for output */
    qsort(s_scenes, s_scene_count, sizeof(scene_entry_t), s_compare_by_note);

    /* Create output directory if needed */
    if (out_dir) {
        MKDIR(out_dir);
    }

    int total_written = 0;

    for (int c = 0; c < channel_count; c++) {
        char filename[512];
        if (out_dir) {
            snprintf(filename, sizeof(filename), "%s/spark-note-names-ch%d.txt",
                out_dir, channels[c] + 1);
        } else {
            snprintf(filename, sizeof(filename), "spark-note-names-ch%d.txt",
                channels[c] + 1);
        }

        FILE *f = fopen(filename, "w");
        if (!f) {
            fprintf(stderr, "error: cannot write '%s'\n", filename);
            return -1;
        }

        int count = 0;
        for (int i = 0; i < s_scene_count; i++) {
            if (s_scenes[i].channel != channels[c]) continue;
            if (!s_scenes[i].enabled && !include_disabled) continue;

            if (include_disabled && !s_scenes[i].enabled) {
                fprintf(f, "%d %s [disabled]\n", s_scenes[i].note, s_scenes[i].name);
            } else {
                fprintf(f, "%d %s\n", s_scenes[i].note, s_scenes[i].name);
            }
            count++;
        }

        fclose(f);
        total_written += count;
        fprintf(stdout, "  ch%d: %s (%d scenes)\n", channels[c] + 1, filename, count);
    }

    fprintf(stdout, "Wrote %d scenes across %d channel(s)\n", total_written, channel_count);
    return 0;
}

/* ---- CLI ---- */

static void usage(void)
{
    fprintf(stderr, "spark-reaper - Generate REAPER integration files from a sparkd project\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  spark-reaper note-names [OPTIONS] <project.yaml>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Subcommands:\n");
    fprintf(stderr, "  note-names    Generate per-channel note-name files\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options (note-names):\n");
    fprintf(stderr, "  -o DIR        Output directory (default: current directory)\n");
    fprintf(stderr, "  --all         Include disabled scenes (suffixed with \" [disabled]\")\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  spark-reaper note-names project.spark.yaml\n");
    fprintf(stderr, "  spark-reaper note-names -o reaper/generated/ project.spark.yaml\n");
}

int main(int argc, char *argv[])
{
    spark_log_init(SPARK_LOG_WARN);

    if (argc < 2) {
        usage();
        return 1;
    }

    if (strcmp(argv[1], "note-names") != 0) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            usage();
            return 0;
        }
        fprintf(stderr, "error: unknown subcommand '%s'\n", argv[1]);
        usage();
        return 1;
    }

    /* Parse note-names options */
    const char *out_dir = NULL;
    const char *project_path = NULL;
    bool include_disabled = false;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: -o requires an argument\n");
                return 1;
            }
            out_dir = argv[++i];
        } else if (strcmp(argv[i], "--all") == 0) {
            include_disabled = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage();
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
            return 1;
        } else {
            if (project_path) {
                fprintf(stderr, "error: multiple project files specified\n");
                return 1;
            }
            project_path = argv[i];
        }
    }

    if (!project_path) {
        fprintf(stderr, "error: no project file specified\n");
        usage();
        return 1;
    }

    if (s_parse_project(project_path) != 0) {
        return 1;
    }

    if (s_scene_count == 0) {
        fprintf(stderr, "warning: no scenes found in project\n");
        return 0;
    }

    return s_write_note_names(out_dir, include_disabled) != 0 ? 1 : 0;
}
