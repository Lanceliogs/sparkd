#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

static spark_log_level_t s_current_level = SPARK_LOG_INFO;

static const char* s_log_silent_str = "SILENT";
static const char* s_log_debug_str = "DEBUG";
static const char* s_log_info_str = "INFO";
static const char* s_log_warn_str = "WARN";
static const char* s_log_error_str = "ERROR";

void spark_log_init(spark_log_level_t level)
{
    s_current_level = level;
}

void spark_log_set_level(spark_log_level_t level)
{
    s_current_level = level;
}

spark_log_level_t spark_log_get_level(void)
{
    return s_current_level;
}

int spark_log_level_from_string(const char *str, spark_log_level_t *out)
{
    if (strcasecmp(str, s_log_silent_str) == 0)
    {
        *out = SPARK_LOG_SILENT;
        return 0;     
    }
    if (strcasecmp(str, s_log_debug_str) == 0)
    {
        *out = SPARK_LOG_DEBUG;
        return 0;     
    }
    if (strcasecmp(str, s_log_info_str) == 0)
    {
        *out = SPARK_LOG_INFO;
        return 0;     
    }
    if (strcasecmp(str, s_log_warn_str) == 0)
    {
        *out = SPARK_LOG_WARN;
        return 0;     
    }
    if (strcasecmp(str, s_log_error_str) == 0)
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
        case SPARK_LOG_SILENT: return s_log_silent_str;
        case SPARK_LOG_DEBUG: return s_log_debug_str;
        case SPARK_LOG_INFO: return s_log_info_str;
        case SPARK_LOG_WARN: return s_log_warn_str;
        case SPARK_LOG_ERROR: return s_log_error_str;
        default: return "UNKNOWN";
    }
}

void spark_log_write(spark_log_level_t level, const char *fmt, ...)
{
    if (s_current_level == SPARK_LOG_SILENT || level < s_current_level)
        return;
    fprintf(stderr, "[%s] ", spark_log_level_to_string(level));
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}
