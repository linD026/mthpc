#ifndef __MTHPC_CENTRALIZED_BARRIER_H__
#define __MTHPC_CENTRALIZED_BARRIER_H__

#include <common.h>
#include <pthread.h>

struct mthpc_barrier {
    int flag;
    int count;
    pthread_mutex_t lock;
} __mthpc_aligned__;

#define MTHPC_BARRIER_INIT                                             \
    {                                                            \
        .flag = 0, .count = 0, .lock = PTHREAD_MUTEX_INITIALIZER \
    }

#define MTHPC_DEFINE_BARRIER(name) struct barrier name = BARRIER_INIT

mthpc_always_inline void mthpc_centralized_barrier(struct barrier *b, size_t n);

#endif /* __MTHPC_CENTRALIZED_BARRIER_H__ */
