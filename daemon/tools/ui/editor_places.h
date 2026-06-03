#ifndef EDITOR_PLACES_H
#define EDITOR_PLACES_H

#include <stdint.h>

#define EDITOR_PLACES_MAX 32
#define EDITOR_PLACE_PATH_MAX 260
#define EDITOR_PLACE_LABEL_MAX 32

typedef struct {
    char label[EDITOR_PLACE_LABEL_MAX];
    char path[EDITOR_PLACE_PATH_MAX];
} editor_place_t;

typedef struct {
    uint8_t project_count;
    editor_place_t projects[EDITOR_PLACES_MAX];
    uint8_t place_count;
    editor_place_t places[EDITOR_PLACES_MAX];
    uint8_t drive_count;
    editor_place_t drives[EDITOR_PLACES_MAX];
} editor_roots_t;

void editor_get_roots(editor_roots_t *roots);

#endif
