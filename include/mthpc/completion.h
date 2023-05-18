#ifndef __MTHPC_COMPLETION_H__
#define __MTHPC_COMPLETION_H__

#include <stdatomic.h>

struct mthpc_completion {
    atomic_ulong cnt;
    atomic_size_t nr;
};

#define MTHPC_COMPLETION_INIT \
    {                         \
        .cnt = 0, nr = 0      \
    }

#define MTHPC_DECLARE_COMPLETION(name, completion) \
    struct mthpc_completion name = MTHPC_COMPLETION_INIT

static inline void mthpc_completion_init(struct mthpc_completion *completion,
                                         unsigned long nr)
{
    completion->cnt = 0;
    completion->nr = nr;
}

static inline void mthpc_complete(struct mthpc_completion *completion)
{
    atomic_fetch_add_explicit(&completion->cnt, 1, memory_order_release);
}

static inline void
mthpc_wait_for_completion(struct mthpc_completion *completion)
{
    while (atomic_load_explicit(&completion->cnt, memory_order_acquire) !=
           completion->nr)
        ;
}

#endif /* __MTHPC_COMPLETION_H__ */
