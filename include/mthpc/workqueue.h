#ifndef __MTHPC_WORKQUEUE_H__
#define __MTHPC_WORKQUEUE_H__

#include <util.h>
#include <list.h>

struct mthpc_workqueue;

struct mthpc_work {
    const char *name;
    void (*func)(struct mthpc_work *);
    void *private;
    unsigned long long padding;
    struct mthpc_list_head node;
    struct mthpc_workqueue *wq;
};

#define MTHPC_DECLARE_WORK(name, func, private) \
    struct mthpc_work name = {                  \
        .name = #name,                          \
        .func = func,                           \
        .private = private,                     \
        .padding = 0,                           \
        .next = NULL,                           \
        .wq = NULL,                             \
    }

int mthpc_queue_work(struct mthpc_work *work);
int mthpc_schedule_work_on(int cpu, struct mthpc_work *work);
mthpc_always_inline void mthpc_dump_work(struct mthpc_work *work);

#endif /* __MTHPC_WORKQUEUE_H__ */
