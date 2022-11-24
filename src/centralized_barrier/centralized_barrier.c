#include <mthpc/centralized_barrier.h>

static __thread int mthpc_local_sense = 0;

mtphc_always_inline void mthpc_centralized_barrier(struct barrier *b, size_t n)
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
