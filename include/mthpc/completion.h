#ifndef __MTHPC_COMPLETION_H__
#define __MTHPC_COMPLETION_H__

#include <stdatomic.h>

/* Reuse the centralized barrier */
#include <mthpc/centralized_barrier.h>

struct mthpc_completion {
    struct mthpc_barrier barrier;
    unsigned long nr;
};

#define MTHPC_COMPLETION_INIT                 \
    {                                         \
        .barrier = MTHPC_BARRIER_INIT, nr = 0 \
    }

#define MTHPC_DECLARE_COMPLETION(name, completion) \
    struct mthpc_completion name = MTHPC_COMPLETION_INIT

static inline void mthpc_completion_init(struct mthpc_completion *completion,
                                         unsigned long nr)
{
    mthpc_barrier_init(&completion->barrier);
    completion->nr = nr;
}

static inline void mthpc_complete(struct mthpc_completion *completion)
{
    mthpc_centralized_barrier_nb(&completion->barrier);
}

static inline void
mthpc_wait_for_completion(struct mthpc_completion *completion)
{
    mthpc_centralized_barrier(&completion->barrier, completion->nr);
}

#endif /* __MTHPC_COMPLETION_H__ */
