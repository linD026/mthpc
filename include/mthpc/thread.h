#ifndef __MTHPC_THREAD_H__
#define __MTHPC_THREAD_H__

#include <mthpc/centralized_barrier.h>
#include <mthpc/rcu.h>

struct mthpc_thread {
    /* Make sure nr member is first member. */
    unsigned int nr;
    void (*init)(struct mthpc_thread *);
    void (*func)(struct mthpc_thread *);
    void *arg;
    struct mthpc_barrier *barrier;
    pthread_t th[];
};

/* Compiler-time declare */

#define MTHPC_DECLARE_THREAD(_name, _nr, _init, _func, _arg) \
    struct __auto_generated__mthpc_thread_##_name {          \
        unsigned int nr;                                     \
        void (*init)(struct mthpc_thread *);                 \
        void (*func)(struct mthpc_thread *);                 \
        void *arg;                                           \
        struct mthpc_barrier *barrier;                       \
        pthread_t th[_nr];                                   \
    } _name = {                                              \
        .nr = _nr,                                           \
        .init = _init,                                       \
        .func = _func,                                       \
        .arg = _arg,                                         \
    }

#define MTHPC_DECLARE_THREADS(_name, ...)                \
    struct __auto_generated_mthpc_threads_##_name {      \
        /* Make sure nr member is first member. */       \
        unsigned int nr;                                 \
        void *thread[macro_var_args_count(__VA_ARGS__)]; \
    } _name = {                                          \
        .nr = macro_var_args_count(__VA_ARGS__),         \
        .thread = { __VA_ARGS__ },                       \
    }

#define mthpc_thread_run(threads)                             \
    do {                                                      \
        __mthpc_thread_run((threads)->thread, (threads)->nr); \
    } while (0)

void *mthpc_thread_worker(void *arg)
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
            pthread_create(&thread->th[j], NULL, mthpc_thread_worker, thread);
    }

    /* Add to workqueue. */
    for (int i = 0; i < nr; i++) {
        struct mthpc_thread *thread = ths[i];
        for (int j = 0; j < thread->nr; j++)
            pthread_join(thread->th[j], NULL);
    }
}

#endif /* __MTHPC_THREAD_H__ */
