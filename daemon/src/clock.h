/*
 * clock.h - Cross-platform monotonic time and sleep
 *
 * Provides monotonic timestamps (ms and us) and sleep functions.
 * Uses clock_gettime/nanosleep on POSIX, QueryPerformanceCounter/Sleep on MSVC.
 * MinGW uses the POSIX path (nanosleep available via winpthreads).
 */
#ifndef SPARK_CLOCK_H
#define SPARK_CLOCK_H

#include <stdint.h>

uint64_t spark_clock_monotonic_ms(void);
uint64_t spark_clock_monotonic_us(void);

void spark_clock_msleep(uint32_t ms);
void spark_clock_usleep(uint32_t us);

#endif