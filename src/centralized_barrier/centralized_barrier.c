#include <mthpc/centralized_barrier.h>
#include <mthpc/debug.h>

#include <internal/feature.h>
#undef _MTHPC_FEATURE
#define _MTHPC_FEATURE centralized_barrier

static __thread int mthpc_local_sense = 0;

void mthpc_centralized_barrier(struct mthpc_barrier *b, size_t n)
{
    mthpc_local_sense = !mthpc_local_sense;

    pthread_mutex_lock(&b->lock);
    b->count++;
    if (b->count == n) {
        b->count = 0;
        pthread_mutex_unlock(&b->lock);
        __atomic_store_n(&b->flag, mthpc_local_sense, __ATOMIC_RELEASE);
    } else {
        pthread_mutex_unlock(&b->lock);
        while (__atomic_load_n(&b->flag, __ATOMIC_ACQUIRE) != mthpc_local_sense)
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
