#ifndef __MTHPC_COMMON_H__
#define __MTHPC_COMMON_H__

/*
 * util.h header helps us to reduce the words. It doesn't means that
 * mthpc supports other compiler and thread library than gcc and pthread.
 */

#define MTHPC_COHERENCE_SIZE 128
#define __mthpc_aligned__ __attribute__((aligned(MTHPC_COHERENCE_SIZE)))

#define mthpc_cmb() __asm__ __volatile__("" : : : "memory")

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#ifndef container_of
#define container_of(ptr, type, member)                        \
    __extension__({                                            \
        const __typeof__(((type *)0)->member) *__mptr = (ptr); \
        (type *)((char *)__mptr - offsetof(type, member));     \
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
#include <stdatomic.h>
#define WRITE_ONCE(x, val)                                                 \
    do {                                                                   \
        mthpc_cmb();                                                       \
        atomic_store_explicit((volatile _Atomic __typeof__(x) *)&x, (val), \
                              memory_order_relaxed);                       \
        mthpc_cmb();                                                       \
    } while (0)
#endif

#ifndef READ_ONCE
#include <stdatomic.h>
#define READ_ONCE(x)                                                      \
    ({                                                                    \
        __typeof__(x) ___x;                                               \
        mthpc_cmb();                                                      \
        ___x = atomic_load_explicit((volatile _Atomic __typeof__(x) *)&x, \
                                    memory_order_consume);                \
        mthpc_cmb();                                                      \
        ___x;                                                             \
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

#ifndef ___PASTE
#define ___PASTE(a, b) a##b
#endif

#ifndef __PASTE
#define __PASTE(a, b) ___PASTE(a, b)
#endif

#ifndef __UNIQUE_ID
#define __UNIQUE_ID(prefix) __PASTE(__PASTE(__UNIQUE_ID_, prefix), __LINE__)
#endif

#endif /* __MTHPC_COMMON_H__ */
