#ifndef __MTHPC_CENTRALIZED_BARRIER_H__
#define __MTHPC_CENTRALIZED_BARRIER_H__

#include <stdatomic.h>

#include <mthpc/spinlock.h>
#include <mthpc/util.h>

struct mthpc_barrier {
    atomic_uint flag;
    int count;
    spinlock_t lock;
} __mthpc_aligned__;

#define MTHPC_BARRIER_INIT                           \
    {                                                \
        .flag = 0, .count = 0, .lock = SPINLOCK_INIT \
    }

#define MTHPC_DEFINE_BARRIER(name) \
    struct mthpc_barrier name = MTHPC_BARRIER_INIT

static inline void mthpc_barrier_init(struct mthpc_barrier *b)
{
    b->flag = 0;
    b->count = 0;
    spin_lock_init(&b->lock);
}

void mthpc_centralized_barrier(struct mthpc_barrier *b, size_t n);

#endif /* __MTHPC_CENTRALIZED_BARRIER_H__ */
