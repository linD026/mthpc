#ifndef __MTHPC_COMPLETION_H__
#define __MTHPC_COMPLETION_H__

#include <stdatomic.h>

/* Reuse the centralized barrier */
#include <mthpc/centralized_barrier.h>

struct mthpc_completion {
    struct mthpc_barrier barrier;
    unsigned long nr;
};

static inline void mthpc_completion_init(struct mthpc_completion *completion, unsigned long nr)
{
    mthpc_barrier_init(&completion->barrier);
    completion->nr = nr;
}

static inline void mthpc_complete(struct mthpc_completion *completion)
{
    mthpc_centralized_barrier_nb(&completion->barrier);
}

static inline void mthpc_wait_for_completion(struct mthpc_completion *completion)
{
    mthpc_centralized_barrier(&completion->barrier, completion->nr);
}

#endif /* __MTHPC_COMPLETION_H__ */
