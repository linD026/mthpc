#ifndef __MTHPC_THREAD_H__
#define __MTHPC_THREAD_H__

#include <stdatomic.h>
#include <pthread.h>

#include <mthpc/list.h>
#include <mthpc/util.h>
#include <mthpc/debug.h>

#define MTHPC_THREAD_GROUP_TYPE 0x0001U
#define MTHPC_THREAD_CLUSTER_TYPE 0x0002U
#define MTHPC_THREAD_ASYNC_TYPE 0x0004U

struct mthpc_barrier_object;

/* Make sure all the members' offset are matched. */
struct mthpc_thread_group {
    unsigned int type;
    unsigned int nr;
    void (*init)(struct mthpc_thread_group *);
    void (*func)(struct mthpc_thread_group *);
    void *args;
    struct mthpc_barrier_object *barrier_object;
    struct mthpc_list_head node;
    atomic_uint nr_exited;
    pthread_t thread[];
};

/* Compiler-time declare */

#define MTHPC_DECLARE_THREAD_GROUP(_name, _nr, _init, _func, _args) \
    struct __auto_generated__mthpc_thread_group_##_name {           \
        unsigned int type;                                          \
        unsigned int nr;                                            \
        void (*init)(struct mthpc_thread_group *);                  \
        void (*func)(struct mthpc_thread_group *);                  \
        void *args;                                                 \
        struct mthpc_barrier_object *barrier_object;                \
        struct mthpc_list_head node;                                \
        atomic_uint nr_exited;                                      \
        pthread_t thread[_nr];                                      \
    } _name = {                                                     \
        .type = MTHPC_THREAD_GROUP_TYPE,                            \
        .nr = _nr,                                                  \
        .init = _init,                                              \
        .func = _func,                                              \
        .args = _args,                                              \
        .nr_exited = 0,                                             \
        .barrier_object = NULL,                                     \
    }

#define MTHPC_DECLARE_THREAD_CLUSTER(_name, ...)           \
    struct __auto_generated_mthpc_thread_cluster_##_name { \
        unsigned int type;                                 \
        unsigned int nr;                                   \
        void *thread[macro_var_args_count(__VA_ARGS__)];   \
    } _name = {                                            \
        .type = MTHPC_THREAD_CLUSTER_TYPE,                 \
        .nr = macro_var_args_count(__VA_ARGS__),           \
        .thread = { __VA_ARGS__ },                         \
    }

void __mthpc_thread_run(void *threads, unsigned int nr);
void __mthpc_thread_async_run(void *threads, unsigned int nr);
void __mthpc_thread_async_wait(void *threads, unsigned int nr);

#define mthpc_thread_run(threads)                                 \
    do {                                                          \
        if ((threads)->type & MTHPC_THREAD_GROUP_TYPE) {          \
            void *__mthpc_ths[1] = { (threads) };                 \
            __mthpc_thread_run(__mthpc_ths, 1);                   \
        } else if ((threads)->type & MTHPC_THREAD_CLUSTER_TYPE)   \
            __mthpc_thread_run((threads)->thread, (threads)->nr); \
        else                                                      \
            MTHPC_WARN_ON(1, "Unexpected thread type");           \
    } while (0)

#define mthpc_thread_async_run(threads)                                 \
    do {                                                                \
        if ((threads)->type & MTHPC_THREAD_GROUP_TYPE) {                \
            void *__mthpc_ths[1] = { (threads) };                       \
            __mthpc_thread_async_run(__mthpc_ths, 1);                   \
        } else if ((threads)->type & MTHPC_THREAD_CLUSTER_TYPE)         \
            __mthpc_thread_async_run((threads)->thread, (threads)->nr); \
        else                                                            \
            MTHPC_WARN_ON(1, "Unexpected thread type");                 \
    } while (0)

#define mthpc_thread_async_wait(threads)                                 \
    do {                                                                 \
        if ((threads)->type & MTHPC_THREAD_GROUP_TYPE) {                 \
            void *__mthpc_ths[1] = { (threads) };                        \
            __mthpc_thread_async_wait(__mthpc_ths, 1);                   \
        } else if ((threads)->type & MTHPC_THREAD_CLUSTER_TYPE)          \
            __mthpc_thread_async_wait((threads)->thread, (threads)->nr); \
        else                                                             \
            MTHPC_WARN_ON(1, "Unexpected thread type");                  \
    } while (0)
#endif /* __MTHPC_THREAD_H__ */
