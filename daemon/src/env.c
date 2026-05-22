#include "env.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define PATH_SEP '/'

#define ENV_MAX_ENTRIES 64
#define ENV_KEY_SIZE 64
#define ENV_VALUE_SIZE 1024

typedef struct {
    char key[ENV_KEY_SIZE];
    char value[ENV_VALUE_SIZE];
} env_entry_t;

static env_entry_t s_entries[ENV_MAX_ENTRIES];
static int s_entry_count = 0;

static int s_parse_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;

    spark_log_info("env: loaded %s", path);

    char line[ENV_KEY_SIZE + ENV_VALUE_SIZE + 2];
    while (fgets(line, sizeof(line), f))
    {
        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;

        /* Strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';

        /* Find '=' separator */
        char *eq = strchr(line, '=');
        if (!eq || eq == line)
            continue;

        if (s_entry_count >= ENV_MAX_ENTRIES)
        {
            spark_log_warn("env: max entries reached (%d), skipping rest", ENV_MAX_ENTRIES);
            break;
        }

        size_t key_len = eq - line;
        if (key_len >= ENV_KEY_SIZE)
            key_len = ENV_KEY_SIZE - 1;

        env_entry_t *entry = &s_entries[s_entry_count];
        memcpy(entry->key, line, key_len);
        entry->key[key_len] = '\0';

        const char *val = eq + 1;
        strncpy(entry->value, val, ENV_VALUE_SIZE - 1);
        entry->value[ENV_VALUE_SIZE - 1] = '\0';

        s_entry_count++;
    }

    fclose(f);
    return 0;
}

static void s_get_home_path(char *buf, size_t size)
{
#ifdef _WIN32
    const char *home = getenv("USERPROFILE");
#else
    const char *home = getenv("HOME");
#endif
    if (home)
        snprintf(buf, size, "%s%c.spark%c.spark.env", home, PATH_SEP, PATH_SEP);
    else
        buf[0] = '\0';
}

int spark_env_load(void)
{
    s_entry_count = 0;

    if (s_parse_file(".spark.env") == 0)
        return 0;

    char home_path[1024];
    s_get_home_path(home_path, sizeof(home_path));
    if (home_path[0] && s_parse_file(home_path) == 0)
        return 0;

    spark_log_debug("env: no .spark.env found (checked cwd and ~/.spark/)");
    return 0;
}

const char *spark_env_get(const char *key)
{
    /* Real env always takes priority */
    const char *val = getenv(key);
    if (val)
        return val;

    /* Fall back to .spark.env entries */
    for (int i = 0; i < s_entry_count; i++)
    {
        if (strcmp(s_entries[i].key, key) == 0)
            return s_entries[i].value;
    }

    return NULL;
}
