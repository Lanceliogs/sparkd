#ifndef SPARK_CLOCK_H
#define SPARK_CLOCK_H

#include <stdint.h>

uint64_t spark_clock_monotonic_ms(void);
uint64_t spark_clock_monotonic_us(void);

void spark_clock_msleep(uint32_t ms);

#endif