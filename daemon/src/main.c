#include "log.h"

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    spark_log_init(SPARK_LOG_INFO);
    spark_log_level_t level = spark_log_get_level();
    spark_log_info("sparkd log init with log level %s", spark_log_level_to_string(level));

    return 0;
}