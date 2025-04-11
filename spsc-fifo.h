/* spsc-fifo.h - v1.0 - Single-Producer Single-Consumer Lock-Free FIFO Queue
                                   no warranty implied; use at your own risk

   This is a single-header library for a lock-free FIFO queue designed
   specifically for single producer thread and single consumer thread
   usecase, e.g. pipeline pattern.

   Usage:
     #define SPSC_FIFO_IMPLEMENTATION
     #include "spsc-fifo.h"

   Options:
     #define SPSC_FIFO_STATIC                - if static functions are preferred
     #define SPSC_FIFO_NDEBUG                - disable thread safety debugging
     #define SPSC_FIFO_ASSERT(expr)          - override default assert implementation (default: assert)
     #define SPSC_FIFO_ALLOC(sz)             - override default memory allocation implementation (default: malloc)
     #define SPSC_FIFO_FREE(p)               - override default memory deallocation implementation (default: free)
     #define SPSC_FIFO_CACHE_LINE_SIZE       - override default cache line size (default: 64 bytes)
     #define SPSC_FIFO_DEFAULT_BUF_ALIGNMENT - override default buffer alignment (default: _Alignof(max_align_t))

   License: MIT (see end of file for license information)
*/

#ifndef SPSC_FIFO_H
#define SPSC_FIFO_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#undef SPSC_FIFO_DEF
#ifdef SPSC_FIFO_STATIC
    #define SPSC_FIFO_DEF static
#else
    #define SPSC_FIFO_DEF extern
#endif

/* Types */
typedef struct spsc_fifo spsc_fifo;
typedef unsigned int     spsc_fifo_usize;
typedef unsigned char    spsc_fifo_byte;

enum spsc_fifo_alloc_status {
    spsc_fifo_alloc_success = 0,
    spsc_fifo_alloc_inval,
    spsc_fifo_alloc_nomem
};

/* General management functions */
SPSC_FIFO_DEF int  spsc_fifo_alloc        (spsc_fifo **fifo, spsc_fifo_usize min_capacity);
SPSC_FIFO_DEF int  spsc_fifo_aligned_alloc(spsc_fifo **fifo, spsc_fifo_usize min_capacity, spsc_fifo_usize buf_alignment);
SPSC_FIFO_DEF void spsc_fifo_free         (spsc_fifo **fifo);
SPSC_FIFO_DEF void spsc_fifo_reset        (spsc_fifo  *fifo);

/* Debugging functions */
SPSC_FIFO_DEF void spsc_fifo_bind_producer(spsc_fifo *fifo);
SPSC_FIFO_DEF void spsc_fifo_bind_consumer(spsc_fifo *fifo);

/* Consumer functions */
SPSC_FIFO_DEF spsc_fifo_usize spsc_fifo_read_avail(spsc_fifo *fifo);
SPSC_FIFO_DEF bool            spsc_fifo_is_empty  (spsc_fifo *fifo);
SPSC_FIFO_DEF spsc_fifo_usize spsc_fifo_skip      (spsc_fifo *fifo, spsc_fifo_usize amount);
SPSC_FIFO_DEF bool            spsc_fifo_skip_n    (spsc_fifo *fifo, spsc_fifo_usize amount);
SPSC_FIFO_DEF spsc_fifo_usize spsc_fifo_read      (spsc_fifo *fifo, spsc_fifo_byte *to, spsc_fifo_usize max);
SPSC_FIFO_DEF bool            spsc_fifo_read_n    (spsc_fifo *fifo, spsc_fifo_byte *to, spsc_fifo_usize len);
SPSC_FIFO_DEF spsc_fifo_usize spsc_fifo_peek      (spsc_fifo *fifo, spsc_fifo_byte *to, spsc_fifo_usize max);
SPSC_FIFO_DEF bool            spsc_fifo_peek_n    (spsc_fifo *fifo, spsc_fifo_byte *to, spsc_fifo_usize len);

/* Producer functions */
SPSC_FIFO_DEF spsc_fifo_usize spsc_fifo_write_avail(spsc_fifo *fifo);
SPSC_FIFO_DEF bool            spsc_fifo_is_full    (spsc_fifo *fifo);
SPSC_FIFO_DEF spsc_fifo_usize spsc_fifo_write      (spsc_fifo *fifo, const spsc_fifo_byte *from, spsc_fifo_usize len);
SPSC_FIFO_DEF bool            spsc_fifo_write_n    (spsc_fifo *fifo, const spsc_fifo_byte *from, spsc_fifo_usize len);

/* Convenience macros for reading/writing typed values (objects) */
#undef spsc_fifo_skip_obj
#define spsc_fifo_skip_obj(fifo_ptr, obj_ptr)  spsc_fifo_skip_n ((fifo_ptr), sizeof(*(obj_ptr)))
#undef spsc_fifo_read_obj
#define spsc_fifo_read_obj(fifo_ptr, obj_ptr)  spsc_fifo_read_n ((fifo_ptr), (spsc_fifo_byte*)(obj_ptr), sizeof(*(obj_ptr)))
#undef spsc_fifo_peek_obj
#define spsc_fifo_peek_obj(fifo_ptr, obj_ptr)  spsc_fifo_peek_n ((fifo_ptr), (spsc_fifo_byte*)(obj_ptr), sizeof(*(obj_ptr)))
#undef spsc_fifo_write_obj
#define spsc_fifo_write_obj(fifo_ptr, obj_ptr) spsc_fifo_write_n((fifo_ptr), (spsc_fifo_byte*)(obj_ptr), sizeof(*(obj_ptr)))

#ifdef __cplusplus
}
#endif

#endif //SPSC_FIFO_H

#ifdef SPSC_FIFO_IMPLEMENTATION

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef uintptr_t spsc_fifo_uptr;

#undef SPSC_FIFO_UTIL
#define SPSC_FIFO_UTIL static inline

#undef SPSC_FIFO_IMPL
#ifdef SPSC_FIFO_STATIC
    #define SPSC_FIFO_IMPL static inline
#else
    #define SPSC_FIFO_IMPL extern inline
#endif

#undef SPSC_FIFO_IGNORE
#define SPSC_FIFO_IGNORE(x) (void)(x)

#ifndef SPSC_FIFO_ASSERT
#define SPSC_FIFO_ASSERT(expr) assert(expr)
#endif

#ifndef SPSC_FIFO_ALLOC
#define SPSC_FIFO_ALLOC(sz) malloc(sz)
#endif

#ifndef SPSC_FIFO_FREE
#define SPSC_FIFO_FREE(p) free(p)
#endif

#ifndef SPSC_FIFO_CACHE_LINE_SIZE
#define SPSC_FIFO_CACHE_LINE_SIZE 64
#endif

#ifndef SPSC_FIFO_DEFAULT_BUF_ALIGNMENT
#define SPSC_FIFO_DEFAULT_BUF_ALIGNMENT _Alignof(max_align_t)
#endif

#undef SPSC_FIFO_ATOMIC
#undef SPSC_FIFO_ATOMIC_LOAD
#undef SPSC_FIFO_ATOMIC_STORE
#undef SPSC_FIFO_MEMORY_ORDER_RELAXED
#undef SPSC_FIFO_MEMORY_ORDER_ACQUIRE
#undef SPSC_FIFO_MEMORY_ORDER_RELEASE
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_ATOMICS__)
    #include <stdatomic.h>
    #define SPSC_FIFO_ATOMIC(type)                      _Atomic(type)
    #define SPSC_FIFO_ATOMIC_LOAD(obj_ptr, order)       atomic_load_explicit ((obj_ptr), (order))
    #define SPSC_FIFO_ATOMIC_STORE(obj_ptr, val, order) atomic_store_explicit((obj_ptr), (val), (order))
    #define SPSC_FIFO_MEMORY_ORDER_RELAXED              memory_order_relaxed
    #define SPSC_FIFO_MEMORY_ORDER_ACQUIRE              memory_order_acquire
    #define SPSC_FIFO_MEMORY_ORDER_RELEASE              memory_order_release
#else
    #ifdef __GNUC__
        #define SPSC_FIFO_ATOMIC(type)                      type
        #define SPSC_FIFO_ATOMIC_LOAD(obj_ptr, order)       __atomic_load_n ((obj_ptr), (order))
        #define SPSC_FIFO_ATOMIC_STORE(obj_ptr, val, order) __atomic_store_n((obj_ptr), (val), (order))
        #define SPSC_FIFO_MEMORY_ORDER_RELAXED              __ATOMIC_RELAXED
        #define SPSC_FIFO_MEMORY_ORDER_ACQUIRE              __ATOMIC_ACQUIRE
        #define SPSC_FIFO_MEMORY_ORDER_RELEASE              __ATOMIC_RELEASE
        #warning "C11 atomics are not available. GCC Compiler detected. Using compiler specific fallback."
    #else
        #define SPSC_FIFO_ATOMIC(type)                      type
        #define SPSC_FIFO_ATOMIC_LOAD(obj_ptr, order)       (*(obj_ptr))
        #define SPSC_FIFO_ATOMIC_STORE(obj_ptr, val, order) (*(obj_ptr) = (val))
        #define SPSC_FIFO_MEMORY_ORDER_RELAXED              0
        #define SPSC_FIFO_MEMORY_ORDER_ACQUIRE              0
        #define SPSC_FIFO_MEMORY_ORDER_RELEASE              0
        #warning "C11 atomics are not available. Using non-thread-safe fallback."
    #endif
#endif

#undef SPSC_FIFO_THREAD_SAFETY_DEBUG
#ifndef SPSC_FIFO_NDEBUG
#define SPSC_FIFO_THREAD_SAFETY_DEBUG
#include <threads.h>
#endif

struct spsc_fifo {
#ifdef SPSC_FIFO_THREAD_SAFETY_DEBUG
    bool producer_bound;
    bool consumer_bound;
    thrd_t producer_thrd;
    thrd_t consumer_thrd;
#endif
    spsc_fifo_usize capacity;
    spsc_fifo_usize mask;
    SPSC_FIFO_ATOMIC(spsc_fifo_usize) write_count;
    spsc_fifo_byte pad1[SPSC_FIFO_CACHE_LINE_SIZE];
    SPSC_FIFO_ATOMIC(spsc_fifo_usize) read_count;
    spsc_fifo_byte pad2[SPSC_FIFO_CACHE_LINE_SIZE];
    spsc_fifo_byte *buf;
};

#ifdef SPSC_FIFO_THREAD_SAFETY_DEBUG
#define SPSC_FIFO_ASSERT_PRODUCER_THREAD(fifo_ptr)                                        \
    do {                                                                                  \
        if ((fifo_ptr)->producer_bound) {                                                 \
            SPSC_FIFO_ASSERT(thrd_equal((fifo_ptr)->producer_thrd, thrd_current()));      \
        }                                                                                 \
    } while (0)
#define SPSC_FIFO_ASSERT_CONSUMER_THREAD(fifo_ptr)                                        \
    do {                                                                                  \
        if ((fifo_ptr)->consumer_bound) {                                                 \
            SPSC_FIFO_ASSERT(thrd_equal((fifo_ptr)->consumer_thrd, thrd_current()));      \
        }                                                                                 \
    } while (0)
#else
#define SPSC_FIFO_ASSERT_PRODUCER_THREAD(fifo_ptr) ((void)0)
#define SPSC_FIFO_ASSERT_CONSUMER_THREAD(fifo_ptr) ((void)0)
#endif

SPSC_FIFO_UTIL bool spsc_fifo_is_pow_2(spsc_fifo_usize x) {
    return x > 0 && (x & x - 1) == 0;
}

SPSC_FIFO_UTIL spsc_fifo_usize spsc_fifo_min(spsc_fifo_usize a, spsc_fifo_usize b) {
    return a < b ? a : b;
}

SPSC_FIFO_UTIL spsc_fifo_usize spsc_fifo_ceil_pow_2(spsc_fifo_usize min) {
    spsc_fifo_usize pow = 1;
    while (min > pow) {
        pow <<= 1;
    }
    return pow;
}

SPSC_FIFO_UTIL spsc_fifo_uptr spsc_fifo_align_backward(spsc_fifo_uptr address, spsc_fifo_usize alignment) {
    return address & ~(alignment - 1);
}

SPSC_FIFO_UTIL spsc_fifo_uptr spsc_fifo_align_forward(spsc_fifo_uptr address, spsc_fifo_usize alignment) {
    return spsc_fifo_align_backward(address + (alignment - 1), alignment);
}

SPSC_FIFO_IMPL int spsc_fifo_alloc(spsc_fifo **fifo, spsc_fifo_usize min_capacity) {
    return spsc_fifo_aligned_alloc(fifo, min_capacity, SPSC_FIFO_DEFAULT_BUF_ALIGNMENT);
}

SPSC_FIFO_IMPL int spsc_fifo_aligned_alloc(spsc_fifo **fifo, spsc_fifo_usize min_capacity, spsc_fifo_usize buf_alignment) {
    if (!spsc_fifo_is_pow_2(buf_alignment)) {
        return spsc_fifo_alloc_inval;
    }

    const spsc_fifo_usize capacity = spsc_fifo_is_pow_2(min_capacity) ? min_capacity : spsc_fifo_ceil_pow_2(min_capacity);

    const spsc_fifo_usize header_size = sizeof(**fifo);
    const spsc_fifo_usize buf_padding = buf_alignment - 1;

    *fifo = SPSC_FIFO_ALLOC(header_size + buf_padding + capacity * sizeof(*(*fifo)->buf));
    if (*fifo == NULL) {
        return spsc_fifo_alloc_nomem;
    }

#ifdef SPSC_FIFO_THREAD_SAFETY_DEBUG
    (*fifo)->producer_bound = false;
    (*fifo)->consumer_bound = false;
#endif
    (*fifo)->capacity = capacity;
    (*fifo)->mask     = capacity - 1;
    SPSC_FIFO_ATOMIC_STORE(&((*fifo)->write_count), 0, SPSC_FIFO_MEMORY_ORDER_RELAXED);
    SPSC_FIFO_ATOMIC_STORE(&((*fifo)->read_count),  0, SPSC_FIFO_MEMORY_ORDER_RELAXED);
    (*fifo)->buf = (spsc_fifo_byte*)spsc_fifo_align_forward((spsc_fifo_uptr)(*fifo) + header_size, buf_alignment);

    return spsc_fifo_alloc_success;
}

SPSC_FIFO_IMPL void spsc_fifo_free(spsc_fifo **fifo) {
    if (*fifo == NULL) {
        return;
    }

    SPSC_FIFO_FREE(*fifo);
    *fifo = NULL;
}

SPSC_FIFO_IMPL void spsc_fifo_reset(spsc_fifo *fifo) {
    SPSC_FIFO_ATOMIC_STORE(&(fifo->write_count), 0, SPSC_FIFO_MEMORY_ORDER_RELAXED);
    SPSC_FIFO_ATOMIC_STORE(&(fifo->read_count),  0, SPSC_FIFO_MEMORY_ORDER_RELAXED);
}

SPSC_FIFO_IMPL void spsc_fifo_bind_producer(spsc_fifo *fifo) {
#ifdef SPSC_FIFO_THREAD_SAFETY_DEBUG
    fifo->producer_thrd  = thrd_current();
    fifo->producer_bound = true;
#else
    SPSC_FIFO_IGNORE(fifo);
#endif
}

SPSC_FIFO_IMPL void spsc_fifo_bind_consumer(spsc_fifo *fifo) {
#ifdef SPSC_FIFO_THREAD_SAFETY_DEBUG
    fifo->consumer_thrd  = thrd_current();
    fifo->consumer_bound = true;
#else
    SPSC_FIFO_IGNORE(fifo);
#endif
}

SPSC_FIFO_IMPL spsc_fifo_usize spsc_fifo_read_avail(spsc_fifo *fifo) {
    SPSC_FIFO_ASSERT_CONSUMER_THREAD(fifo);

    const spsc_fifo_usize write_count = SPSC_FIFO_ATOMIC_LOAD(&(fifo->write_count), SPSC_FIFO_MEMORY_ORDER_ACQUIRE);
    const spsc_fifo_usize read_count  = SPSC_FIFO_ATOMIC_LOAD(&(fifo->read_count),  SPSC_FIFO_MEMORY_ORDER_RELAXED);

    return write_count - read_count;
}

SPSC_FIFO_IMPL bool spsc_fifo_is_empty(spsc_fifo *fifo) {
    return spsc_fifo_read_avail(fifo) == 0;
}

SPSC_FIFO_IMPL spsc_fifo_usize spsc_fifo_skip(spsc_fifo *fifo, spsc_fifo_usize amount) {
    SPSC_FIFO_ASSERT_CONSUMER_THREAD(fifo);

    const spsc_fifo_usize write_count = SPSC_FIFO_ATOMIC_LOAD(&(fifo->write_count), SPSC_FIFO_MEMORY_ORDER_ACQUIRE);
    const spsc_fifo_usize read_count  = SPSC_FIFO_ATOMIC_LOAD(&(fifo->read_count),  SPSC_FIFO_MEMORY_ORDER_RELAXED);
    const spsc_fifo_usize read_avail  = write_count - read_count;
    if (amount > read_avail) {
        amount = read_avail;
    }

    if (amount == 0) {
        return 0;
    }

    SPSC_FIFO_ATOMIC_STORE(&(fifo->read_count), read_count + amount, SPSC_FIFO_MEMORY_ORDER_RELEASE);

    return amount;
}

SPSC_FIFO_IMPL bool spsc_fifo_skip_n(spsc_fifo *fifo, spsc_fifo_usize amount) {
    SPSC_FIFO_ASSERT_CONSUMER_THREAD(fifo);

    const spsc_fifo_usize write_count = SPSC_FIFO_ATOMIC_LOAD(&(fifo->write_count), SPSC_FIFO_MEMORY_ORDER_ACQUIRE);
    const spsc_fifo_usize read_count  = SPSC_FIFO_ATOMIC_LOAD(&(fifo->read_count),  SPSC_FIFO_MEMORY_ORDER_RELAXED);
    const spsc_fifo_usize read_avail  = write_count - read_count;
    if (amount == 0 || amount > read_avail) {
        return false;
    }

    SPSC_FIFO_ATOMIC_STORE(&(fifo->read_count), read_count + amount, SPSC_FIFO_MEMORY_ORDER_RELEASE);

    return true;
}

SPSC_FIFO_IMPL spsc_fifo_usize spsc_fifo_read(spsc_fifo *fifo, spsc_fifo_byte *to, spsc_fifo_usize max) {
    SPSC_FIFO_ASSERT_CONSUMER_THREAD(fifo);

    const spsc_fifo_usize write_count = SPSC_FIFO_ATOMIC_LOAD(&(fifo->write_count), SPSC_FIFO_MEMORY_ORDER_ACQUIRE);
    const spsc_fifo_usize read_count  = SPSC_FIFO_ATOMIC_LOAD(&(fifo->read_count),  SPSC_FIFO_MEMORY_ORDER_RELAXED);

    const spsc_fifo_usize read_avail = write_count - read_count;
    if (max > read_avail) {
        max = read_avail;
    }

    if (max == 0) {
        return 0;
    }

    const spsc_fifo_usize read_idx = read_count & fifo->mask;
    const spsc_fifo_usize len = spsc_fifo_min(max, fifo->capacity - read_idx);
    memcpy(to, fifo->buf + read_idx, len);
    if (len < max) {
        memcpy(to + len, fifo->buf, max - len);
    }

    SPSC_FIFO_ATOMIC_STORE(&(fifo->read_count), read_count + max, SPSC_FIFO_MEMORY_ORDER_RELEASE);

    return max;
}

SPSC_FIFO_IMPL bool spsc_fifo_read_n(spsc_fifo *fifo, spsc_fifo_byte *to, spsc_fifo_usize len) {
    SPSC_FIFO_ASSERT_CONSUMER_THREAD(fifo);

    const spsc_fifo_usize write_count = SPSC_FIFO_ATOMIC_LOAD(&(fifo->write_count), SPSC_FIFO_MEMORY_ORDER_ACQUIRE);
    const spsc_fifo_usize read_count  = SPSC_FIFO_ATOMIC_LOAD(&(fifo->read_count),  SPSC_FIFO_MEMORY_ORDER_RELAXED);

    const spsc_fifo_usize read_avail = write_count - read_count;
    if (len == 0 || len > read_avail) {
        return false;
    }

    const spsc_fifo_usize read_idx = read_count & fifo->mask;
    const spsc_fifo_usize l = spsc_fifo_min(len, fifo->capacity - read_idx);
    memcpy(to, fifo->buf + read_idx, l);
    if (l < len) {
        memcpy(to + l, fifo->buf, len - l);
    }

    SPSC_FIFO_ATOMIC_STORE(&(fifo->read_count), read_count + len, SPSC_FIFO_MEMORY_ORDER_RELEASE);

    return true;
}

SPSC_FIFO_IMPL spsc_fifo_usize spsc_fifo_peek(spsc_fifo *fifo, spsc_fifo_byte *to, spsc_fifo_usize max) {
    SPSC_FIFO_ASSERT_CONSUMER_THREAD(fifo);

    const spsc_fifo_usize write_count = SPSC_FIFO_ATOMIC_LOAD(&(fifo->write_count), SPSC_FIFO_MEMORY_ORDER_ACQUIRE);
    const spsc_fifo_usize read_count  = SPSC_FIFO_ATOMIC_LOAD(&(fifo->read_count),  SPSC_FIFO_MEMORY_ORDER_RELAXED);

    const spsc_fifo_usize read_avail = write_count - read_count;
    if (max > read_avail) {
        max = read_avail;
    }

    if (max == 0) {
        return 0;
    }

    const spsc_fifo_usize read_idx = read_count & fifo->mask;
    const spsc_fifo_usize len = spsc_fifo_min(max, fifo->capacity - read_idx);
    memcpy(to, fifo->buf + read_idx, len);
    if (len < max) {
        memcpy(to + len, fifo->buf, max - len);
    }

    return max;
}

SPSC_FIFO_IMPL bool spsc_fifo_peek_n(spsc_fifo *fifo, spsc_fifo_byte *to, spsc_fifo_usize len) {
    SPSC_FIFO_ASSERT_CONSUMER_THREAD(fifo);

    const spsc_fifo_usize write_count = SPSC_FIFO_ATOMIC_LOAD(&(fifo->write_count), SPSC_FIFO_MEMORY_ORDER_ACQUIRE);
    const spsc_fifo_usize read_count  = SPSC_FIFO_ATOMIC_LOAD(&(fifo->read_count),  SPSC_FIFO_MEMORY_ORDER_RELAXED);

    const spsc_fifo_usize read_avail = write_count - read_count;
    if (len == 0 || len > read_avail) {
        return false;
    }

    const spsc_fifo_usize read_idx = read_count & fifo->mask;
    const spsc_fifo_usize l = spsc_fifo_min(len, fifo->capacity - read_idx);
    memcpy(to, fifo->buf + read_idx, l);
    if (l < len) {
        memcpy(to + l, fifo->buf, len - l);
    }

    return true;
}

SPSC_FIFO_IMPL spsc_fifo_usize spsc_fifo_write_avail(spsc_fifo *fifo) {
    SPSC_FIFO_ASSERT_PRODUCER_THREAD(fifo);

    const spsc_fifo_usize read_count  = SPSC_FIFO_ATOMIC_LOAD(&(fifo->read_count),  SPSC_FIFO_MEMORY_ORDER_ACQUIRE);
    const spsc_fifo_usize write_count = SPSC_FIFO_ATOMIC_LOAD(&(fifo->write_count), SPSC_FIFO_MEMORY_ORDER_RELAXED);

    return fifo->capacity - (write_count - read_count);
}

SPSC_FIFO_IMPL bool spsc_fifo_is_full(spsc_fifo *fifo) {
    return spsc_fifo_write_avail(fifo) == 0;
}

SPSC_FIFO_IMPL spsc_fifo_usize spsc_fifo_write(spsc_fifo *fifo, const spsc_fifo_byte *from, spsc_fifo_usize len) {
    SPSC_FIFO_ASSERT_PRODUCER_THREAD(fifo);

    const spsc_fifo_usize read_count  = SPSC_FIFO_ATOMIC_LOAD(&(fifo->read_count),  SPSC_FIFO_MEMORY_ORDER_ACQUIRE);
    const spsc_fifo_usize write_count = SPSC_FIFO_ATOMIC_LOAD(&(fifo->write_count), SPSC_FIFO_MEMORY_ORDER_RELAXED);

    const spsc_fifo_usize write_avail = fifo->capacity - (write_count - read_count);
    if (len > write_avail) {
        len = write_avail;
    }

    if (len == 0) {
        return 0;
    }

    const spsc_fifo_usize write_idx = write_count & fifo->mask;
    const spsc_fifo_usize l = spsc_fifo_min(len, fifo->capacity - write_idx);
    memcpy(fifo->buf + write_idx, from, l);
    if (l < len) {
        memcpy(fifo->buf, from + l, len - l);
    }

    SPSC_FIFO_ATOMIC_STORE(&(fifo->write_count), write_count + len, SPSC_FIFO_MEMORY_ORDER_RELEASE);

    return len;
}

SPSC_FIFO_IMPL bool spsc_fifo_write_n(spsc_fifo *fifo, const spsc_fifo_byte *from, spsc_fifo_usize len) {
    SPSC_FIFO_ASSERT_PRODUCER_THREAD(fifo);

    const spsc_fifo_usize read_count  = SPSC_FIFO_ATOMIC_LOAD(&(fifo->read_count),  SPSC_FIFO_MEMORY_ORDER_ACQUIRE);
    const spsc_fifo_usize write_count = SPSC_FIFO_ATOMIC_LOAD(&(fifo->write_count), SPSC_FIFO_MEMORY_ORDER_RELAXED);

    const spsc_fifo_usize write_avail = fifo->capacity - (write_count - read_count);
    if (len == 0 || len > write_avail) {
        return false;
    }

    const spsc_fifo_usize write_idx = write_count & fifo->mask;
    const spsc_fifo_usize l = spsc_fifo_min(len, fifo->capacity - write_idx);
    memcpy(fifo->buf + write_idx, from, l);
    if (l < len) {
        memcpy(fifo->buf, from + l, len - l);
    }

    SPSC_FIFO_ATOMIC_STORE(&(fifo->write_count), write_count + len, SPSC_FIFO_MEMORY_ORDER_RELEASE);

    return true;
}

#endif //SPSC_FIFO_IMPLEMENTATION

/*
   Copyright 2025 Karlo Bratko <kbratko@tuta.io>

   Permission is hereby granted, free of charge, to any person obtaining a copy of this software
   and associated documentation files (the “Software”), to deal in the Software without
   restriction, including without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all copies or
   substantial portions of the Software.

   THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/