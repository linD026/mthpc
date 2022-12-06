#include <pthread.h>

#include <mthpc/thread.h>
#include <mthpc/centralized_barrier.h>
#include <mthpc/rcu.h>

#include <internal/feature.h>
#undef _MTHPC_FEATURE
#define _MTHPC_FEATURE thread

static void *mthpc_thread_worker(void *arg)
{
    struct mthpc_thread *thread = arg;

    mthpc_rcu_thread_init();
    if (thread->init)
        thread->init(thread);
    if (thread->barrier)
        mthpc_centralized_barrier(thread->barrier, thread->nr);
    if (thread->func)
        thread->func(thread);
    mthpc_rcu_thread_exit();

    return NULL;
}

void __mthpc_thread_run(void *threads, unsigned int nr)
{
    struct mthpc_thread **ths = (struct mthpc_thread **)threads;
    struct mthpc_barrier barrier = MTHPC_BARRIER_INIT;

    for (int i = 0; i < nr; i++) {
        struct mthpc_thread *thread = ths[i];
        thread->barrier = &barrier;
        for (int j = 0; j < thread->nr; j++)
            pthread_create(&thread->thread[j], NULL, mthpc_thread_worker,
                           thread);
    }

    /* Add to workqueue. */
    for (int i = 0; i < nr; i++) {
        struct mthpc_thread *thread = ths[i];
        for (int j = 0; j < thread->nr; j++)
            pthread_join(thread->thread[j], NULL);
    }
}

static __mthpc_init void mthpc_thread_init(void)
{
    mthpc_init_feature();
    mthpc_init_ok();
}

static __mthpc_exit void mthpc_thread_exit(void)
{
    mthpc_exit_feature();
    mthpc_exit_ok();
}
