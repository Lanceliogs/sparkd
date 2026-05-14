#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

static spark_log_level_t current_level = SPARK_LOG_INFO;

static const char* SPARK_LOG_DEBUG_STR = "DEBUG";
static const char* SPARK_LOG_INFO_STR = "INFO";
static const char* SPARK_LOG_WARN_STR = "WARN";
static const char* SPARK_LOG_ERROR_STR = "ERROR";

void spark_log_init(spark_log_level_t level)
{
    current_level = level;
}

void spark_log_set_level(spark_log_level_t level)
{
    current_level = level;
}

spark_log_level_t spark_log_get_level(void)
{
    return current_level;
}

int spark_log_level_from_string(const char *str, spark_log_level_t *out)
{
    if (strcasecmp(str, SPARK_LOG_DEBUG_STR) == 0)
    {
        *out = SPARK_LOG_DEBUG;
        return 0;     
    }
    if (strcasecmp(str, SPARK_LOG_INFO_STR) == 0)
    {
        *out = SPARK_LOG_INFO;
        return 0;     
    }
    if (strcasecmp(str, SPARK_LOG_WARN_STR) == 0)
    {
        *out = SPARK_LOG_WARN;
        return 0;     
    }
    if (strcasecmp(str, SPARK_LOG_ERROR_STR) == 0)
    {
        *out = SPARK_LOG_ERROR;
        return 0;     
    }
    return -1;
}

const char *spark_log_level_to_string(spark_log_level_t level)
{
    switch (level)
    {
        case SPARK_LOG_DEBUG:
            return SPARK_LOG_DEBUG_STR;
        case SPARK_LOG_INFO:
            return SPARK_LOG_INFO_STR;
        case SPARK_LOG_WARN:
            return SPARK_LOG_WARN_STR;
        case SPARK_LOG_ERROR:
            return SPARK_LOG_ERROR_STR;
        default:
            return "UNKNOWN";
    }
}

void spark_log_write(spark_log_level_t level, const char *fmt, ...)
{
    if (level < current_level)
        return;
    fprintf(stderr, "[%s] ", spark_log_level_to_string(level));
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}
