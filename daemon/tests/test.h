#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <string.h>

static int test_count;
static int test_failures;

#define TEST_BEGIN() \
    do { test_count = 0; test_failures = 0; } while (0)

#define TEST_END() \
    do { \
        printf("\n%d/%d tests passed\n", \
               test_count - test_failures, test_count); \
        return test_failures > 0 ? 1 : 0; \
    } while (0)

#define RUN_TEST(fn) \
    do { \
        int _before = test_failures; \
        test_count++; \
        printf("  %-40s ", #fn); \
        fn(); \
        if (test_failures == _before) \
            printf("OK\n"); \
    } while (0)

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            printf("FAIL\n    %s:%d: %s\n", \
                   __FILE__, __LINE__, #expr); \
            test_failures++; \
            return; \
        } \
    } while (0)

#define ASSERT_EQ(a, b) \
    do { \
        long long _a = (long long)(a); \
        long long _b = (long long)(b); \
        if (_a != _b) { \
            printf("FAIL\n    %s:%d: %s == %lld, expected %lld\n", \
                   __FILE__, __LINE__, #a, _a, _b); \
            test_failures++; \
            return; \
        } \
    } while (0)

#define ASSERT_STR_EQ(a, b) \
    do { \
        const char *_a = (a); \
        const char *_b = (b); \
        if (strcmp(_a, _b) != 0) { \
            printf("FAIL\n    %s:%d: \"%s\" != \"%s\"\n", \
                   __FILE__, __LINE__, _a, _b); \
            test_failures++; \
            return; \
        } \
    } while (0)

#endif
