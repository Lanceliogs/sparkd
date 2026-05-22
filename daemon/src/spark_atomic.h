/*
 * spark_atomic.h - Lightweight atomic helpers for stats and state
 *
 * All operations use relaxed memory ordering, suitable for independent
 * counters and status flags read from a different thread for display.
 */
#ifndef SPARK_ATOMIC_H
#define SPARK_ATOMIC_H

#include <stdatomic.h>
#include <stdint.h>

static inline uint64_t spark_atomic_load_u64(const volatile _Atomic uint64_t *v)
{
    return atomic_load_explicit((_Atomic uint64_t *)v, memory_order_relaxed);
}

static inline void spark_atomic_store_u64(volatile _Atomic uint64_t *v, uint64_t val)
{
    atomic_store_explicit(v, val, memory_order_relaxed);
}

static inline void spark_atomic_inc(volatile _Atomic uint64_t *v)
{
    atomic_fetch_add_explicit(v, 1, memory_order_relaxed);
}

#define spark_atomic_load(ptr) \
    atomic_load_explicit(ptr, memory_order_relaxed)

#define spark_atomic_store(ptr, val) \
    atomic_store_explicit(ptr, val, memory_order_relaxed)

#endif
