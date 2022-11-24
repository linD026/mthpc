#ifndef __MTHPC_COMMON_H__
#define __MTHPC_COMMON_H__

/*
 * common.h header helps us to reduce the words. It doesn't means that
 * mpthc supports other compiler and thread library than gcc and pthread.
 */

#define MTHPC_COHERENCE_SIZE 128
#define __mthpc_aligned__  __attribute__((aligned(MTHPC_COHERENCE_SIZE)))

#define mthpc_barrier() asm volatile("" : : : "memory")

#define mthpc_likely(x) __builtin_expect(!!(x), 1)
#define mthpc_unlikely(x) __builtin_expect(!!(x), 0)

#define mthpc_always_inline inline __attribute__((__always_inline__))

#define mthpc_noinline __attribute__((__noinline__))

#endif /* __MTHPC_COMMON_H__ */
