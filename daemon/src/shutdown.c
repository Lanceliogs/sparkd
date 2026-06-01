#include "shutdown.h"
#include <stdint.h>

static volatile uint8_t s_shutdown_requested = 0;

void spark_request_shutdown(void)
{
    s_shutdown_requested = 1;
}

int spark_shutdown_requested(void)
{
    return s_shutdown_requested;
}
