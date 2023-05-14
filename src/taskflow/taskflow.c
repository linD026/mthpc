#include <stdlib.h>

#include <mthpc/taskflow.h>
#include <mthpc/spinlock.h>
#include <mthpc/debug.h>
#include <mthpc/util.h>

#include <internal/feature.h>
#undef _MTHPC_FEATURE
#define _MTHPC_FEATURE taskflow

struct mthpc_task {
    void (*func)(void);
    struct mthpc_edge {
        struct mthpc_task **node;
        int nr_node;
        int size;
        spinlock_t lock;
    } in_edge, out_edge;
};

void __mthpc_taskflow_precede(struct mthpc_task *task, struct mthpc_task *tasks,
                              int nr_task);

void __mthpc_taskflow_succeed(struct mthpc_task *task, struct mthpc_task *tasks,
                              int nr_task);

struct mthpc_taskflow *mthpc_taskflow_create(void)
{
}

static void __mthpc_init mthpc_taskflow_init(void)
{
    mthpc_init_feature();
    mthpc_init_ok();
}

static void __mthpc_exit mthpc_taskflow_exit(void)
{
    mthpc_exit_feature();
    mthpc_exit_ok();
}
