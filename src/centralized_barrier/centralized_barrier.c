#include <stdatomic.h>

#include <mthpc/centralized_barrier.h>
#include <mthpc/spinlock.h>
#include <mthpc/debug.h>

#include <internal/feature.h>
#undef _MTHPC_FEATURE
#define _MTHPC_FEATURE centralized_barrier

static __thread int mthpc_local_sense = 0;

void mthpc_centralized_barrier(struct mthpc_barrier *b, size_t n)
{
    mthpc_local_sense = !mthpc_local_sense;

    spin_lock(&b->lock);
    b->count++;
    if (b->count == n) {
        b->count = 0;
        spin_unlock(&b->lock);
        atomic_store_explicit(&b->flag, mthpc_local_sense,
                              memory_order_release);
    } else {
        spin_unlock(&b->lock);
        while (atomic_load_explicit(&b->flag, memory_order_acquire) !=
               mthpc_local_sense)
            ;
    }
}

static __mthpc_init void mthpc_centralized_barrier_init(void)
{
    mthpc_init_feature();
    mthpc_init_ok();
}

static __mthpc_exit void mthpc_centralized_barrier_exit(void)
{
    mthpc_exit_feature();
    mthpc_exit_ok();
}
