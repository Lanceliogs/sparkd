#include "../project.h"
#include "../fixture.h"
#include "../scene.h"
#include "../log.h"

#include "yaml.h"

static int s_load_hardcoded(void)
{
    spark_scene_value_def_t scene_values[] = {
        { .dmx_index = 0, .value = 255, .velocity_scaling = false },
        { .dmx_index = 1, .value = 255, .velocity_scaling = false },
        { .dmx_index = 5, .value = 0,   .velocity_scaling = false },
    };

    spark_scene_def_t scene_def = {
        .channel = 0, .note = 60,
        .id = "red-light-district", .name = "Red Light District",
        .comment = "Sexy red light, ou yeah",
        .enabled = true,
        .trigger_mode = SPARK_SCENE_GATE,
        .output_mode = SPARK_SCENE_STATIC,
        .values = scene_values, .value_count = 3,
    };

    if (spark_scene_add_def(&scene_def) != 0)
        return -1;

    spark_log_info("project: loaded hardcoded fallback mapping");
    return 0;
}

int spark_project_load(const char *path)
{
    spark_fixture_reset();
    spark_scene_reset();

    int rc;

    if (!path)
    {
        rc = s_load_hardcoded();
    }
    else
    {
        rc = spark_project_parse_yaml(path);
    }

    if (rc != 0)
        return rc;

    rc = spark_scene_resolve();
    if (rc != 0)
    {
        spark_log_error("project: scene resolution failed");
        return rc;
    }

    spark_log_info("project: loaded and resolved");
    return 0;
}
