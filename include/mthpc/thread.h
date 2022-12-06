#ifndef __MTHPC_THREAD_H__
#define __MTHPC_THREAD_H__

#include <pthread.h>

#include <mthpc/util.h>
#include <mthpc/debug.h>

#define MTHPC_THREAD_TYPE 0x0001U
#define MTHPC_THREADS_TYPE 0x002U

/* Make sure all the members' offset are matched. */
struct mthpc_thread {
    unsigned int type;
    unsigned int nr;
    void (*init)(struct mthpc_thread *);
    void (*func)(struct mthpc_thread *);
    void *args;
    struct mthpc_barrier *barrier;
    pthread_t thread[];
};

/* Compiler-time declare */

#define MTHPC_DECLARE_THREAD(_name, _nr, _init, _func, _args) \
    struct __auto_generated__mthpc_thread_##_name {           \
        unsigned int type;                                    \
        unsigned int nr;                                      \
        void (*init)(struct mthpc_thread *);                  \
        void (*func)(struct mthpc_thread *);                  \
        void *args;                                           \
        struct mthpc_barrier *barrier;                        \
        pthread_t thread[_nr];                                \
    } _name = {                                               \
        .type = MTHPC_THREAD_TYPE,                            \
        .nr = _nr,                                            \
        .init = _init,                                        \
        .func = _func,                                        \
        .args = _args,                                        \
    }

#define MTHPC_DECLARE_THREADS(_name, ...)                \
    struct __auto_generated_mthpc_threads_##_name {      \
        unsigned int type;                               \
        unsigned int nr;                                 \
        void *thread[macro_var_args_count(__VA_ARGS__)]; \
    } _name = {                                          \
        .type = MTHPC_THREADS_TYPE,                      \
        .nr = macro_var_args_count(__VA_ARGS__),         \
        .thread = { __VA_ARGS__ },                       \
    }

#define mthpc_thread_run(threads)                                 \
    do {                                                          \
        if ((threads)->type & MTHPC_THREAD_TYPE) {                \
            void *__mthpc_ths[1] = { (threads) };                 \
            __mthpc_thread_run(__mthpc_ths, 1);                   \
        } else if ((threads)->type & MTHPC_THREADS_TYPE)          \
            __mthpc_thread_run((threads)->thread, (threads)->nr); \
        else                                                      \
            MTHPC_WARN_ON(1, "Unexpected thread type");           \
    } while (0)

void __mthpc_thread_run(void *threads, unsigned int nr);

#endif /* __MTHPC_THREAD_H__ */
