#include "clock.h"

#ifdef _MSC_VER
#include <windows.h>
#else
#include <time.h>
#endif

uint64_t spark_clock_monotonic_ms(void)
{
#ifdef _MSC_VER
    LARGE_INTEGER freq, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (uint64_t)(now.QuadPart * 1000 / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}

uint64_t spark_clock_monotonic_us(void)
{
#ifdef _MSC_VER
    LARGE_INTEGER freq, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (uint64_t)(now.QuadPart * 1000000 / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
#endif
}

void spark_clock_msleep(uint32_t ms)
{
#ifdef _MSC_VER
    Sleep(ms); /* windows.h */
#else
    struct timespec req = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000 };
    nanosleep(&req, NULL);
#endif
}

void spark_clock_usleep(uint32_t us)
{
#ifdef _MSC_VER
    uint32_t ms = us / 1000;
    if (ms < 1) ms = 1; 
    Sleep(ms); /* windows.h */
#else
    struct timespec req = { .tv_sec = us / 1000000, .tv_nsec = (us % 1000000) * 1000 };
    nanosleep(&req, NULL);
#endif
}
