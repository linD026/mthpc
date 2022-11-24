#ifndef __MTHPC_COMMON_H__
#define __MTHPC_COMMON_H__

/*
 * common.h header helps us to reduce the words. It doesn't means that
 * mthpc supports other compiler and thread library than gcc and pthread.
 */

#define MTHPC_COHERENCE_SIZE 128
#define __mthpc_aligned__ __attribute__((aligned(MTHPC_COHERENCE_SIZE)))

#define mthpc_cmb() asm volatile("" : : : "memory")

#define mthpc_likely(x) __builtin_expect(!!(x), 1)
#define mthpc_unlikely(x) __builtin_expect(!!(x), 0)

#ifndef container_of
#define container_of(ptr, type, member)                    \
    ({                                                     \
        const typeof(((type *)0)->member) *__mptr = (ptr); \
        (type *)((char *)__mptr - offsetof(type, member)); \
    })
#endif

#define mthpc_always_inline inline __attribute__((__always_inline__))

#define mthpc_noinline __attribute__((__noinline__))

#define mthpc_init __attribute__((constructor))
#define mthpc_exit __attribute__((destructor))

#endif /* __MTHPC_COMMON_H__ */
