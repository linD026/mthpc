#ifndef __MTHPC_COMMON_H__
#define __MTHPC_COMMON_H__

/*
 * common.h header helps us to reduce the words. It doesn't means that
 * mthpc supports other compiler and thread library than gcc and pthread.
 */

#define MTHPC_COHERENCE_SIZE 128
#define __mthpc_aligned__ __attribute__((aligned(MTHPC_COHERENCE_SIZE)))

#define mthpc_cmb() asm volatile("" : : : "memory")

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#ifndef container_of
#define container_of(ptr, type, member)                    \
    ({                                                     \
        const typeof(((type *)0)->member) *__mptr = (ptr); \
        (type *)((char *)__mptr - offsetof(type, member)); \
    })
#endif

#ifndef __always_inline
#define __always_inline inline __attribute__((__always_inline__))
#endif

#ifndef __noinline
#define __noinline __attribute__((__noinline__))
#endif

#ifndef __allow_unused
#define __allow_unused __attribute__((unused))
#endif

#ifndef WRITE_ONCE
#define WRITE_ONCE(x, val)                             \
    do {                                               \
        mthpc_cmb();                                   \
        __atomic_store_n(&(x), val, __ATOMIC_RELAXED); \
        mthpc_cmb();                                   \
    } while (0)
#endif

#ifndef READ_ONCE
#define READ_ONCE(x)                                    \
    ({                                                  \
        typeof(x) ___x;                                 \
        mthpc_cmb();                                    \
        ___x = __atomic_load_n(&(x), __ATOMIC_CONSUME); \
        mthpc_cmb();                                    \
        ___x;                                           \
    })
#endif

#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#include <stdatomic.h>
#define smp_mb() atomic_thread_fence(memory_order_seq_cst)
#endif
#endif

#ifndef smp_mb
#if defined(__GNUC__) && __SANITIZE_THREAD__
#include <stdatomic.h>
#define smp_mb() atomic_thread_fence(memory_order_seq_cst)
#endif
#endif

/* ThreadSanitizer doesn't support __atomic_thread_fence(__ATOMIC_SEQ_CST) */
#ifndef smp_mb
#define smp_mb() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#endif

#ifndef macro_var_args_count
#define macro_var_args_count(...) \
    (sizeof((void *[]){ 0, __VA_ARGS__ }) / sizeof(void *) - 1)
#endif

#endif /* __MTHPC_COMMON_H__ */
