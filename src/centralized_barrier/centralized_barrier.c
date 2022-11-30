#include <mthpc/centralized_barrier.h>
#include <mthpc/debug.h>

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

static mthpc_init(mthpc_indep) void mthpc_centralized_barrier_init(void)
{
    mthpc_print("centralized barrier init\n");
}

static mthpc_exit(mthpc_indep) void mthpc_centralized_barrier_exit(void)
{
    mthpc_print("centralized barrier exit\n");
}
