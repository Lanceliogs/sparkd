#ifndef SPARK_LOG_H
#define SPARK_LOG_H

typedef enum {
    SPARK_LOG_DEBUG,
    SPARK_LOG_INFO,
    SPARK_LOG_WARN,
    SPARK_LOG_ERROR
} spark_log_level_t;

void spark_log_init(spark_log_level_t level);
void spark_log_set_level(spark_log_level_t level);
spark_log_level_t spark_log_get_level(void);

/* Returns -1 if the string is not recognized */
int spark_log_level_from_string(const char *str, spark_log_level_t *out);
const char *spark_log_level_to_string(spark_log_level_t level);

void spark_log_write(spark_log_level_t level, const char *fmt, ...);

#define spark_log_debug(...) spark_log_write(SPARK_LOG_DEBUG, __VA_ARGS__)
#define spark_log_info(...)  spark_log_write(SPARK_LOG_INFO,  __VA_ARGS__)
#define spark_log_warn(...)  spark_log_write(SPARK_LOG_WARN,  __VA_ARGS__)
#define spark_log_error(...) spark_log_write(SPARK_LOG_ERROR, __VA_ARGS__)

#endif
