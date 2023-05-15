#include <stdlib.h>

#include <mthpc/taskflow.h>
#include <mthpc/list.h>
#include <mthpc/completion.h>
#include <mthpc/workqueue.h>
#include <mthpc/spinlock.h>
#include <mthpc/debug.h>
#include <mthpc/util.h>

#include <internal/workqueue.h>

#include <internal/feature.h>
#undef _MTHPC_FEATURE
#define _MTHPC_FEATURE taskflow

struct mthpc_task {
    struct mthpc_work work;
    struct mthpc_taskflow *parent;
    void (*func)(void *);
    struct mthpc_list_head list_node;

    unsigned long nr_internal;
    struct mthpc_list_head internal_list_head;
    struct mthpc_list_head internal_list_node;
};
struct mthpc_taskflow {
    struct mthpc_list_head list_head;
    int nr_task;
    struct mthpc_completion completion;
};

static void mthpc_task_worker(struct mthpc_work *work)
{
    struct mthpc_task *task = container_of(work, struct mthpc_task, work);
    struct mthpc_taskflow *tf = task->parent;
    task->func(work->private);
    mthpc_complete(&tf->completion);
}

/* user API */

void __mthpc_taskflow_precede(struct mthpc_task *task, struct mthpc_task *tasks,
                              int nr_task);

void __mthpc_taskflow_succeed(struct mthpc_task *task, struct mthpc_task *tasks,
                              int nr_task);

struct mthpc_taskflow *mthpc_taskflow_create(void (*start)(void *arg),
                                             void *arg)
{
    struct mthpc_taskflow *tf = malloc(sizeof(struct mthpc_taskflow));
    struct mthpc_task *task;
    if (!tf) {
        MTHPC_WARN_ON(1, "allocate taskflow failed");
        return NULL;
    }

    mthpc_list_init(&tf->list_head);
    tf->nr_task = 0;
    mthpc_completion_init(&tf->completion, 1);

    task = malloc(sizeof(struct mthpc_task));
    if (!task) {
        MTHPC_WARN_ON(1, "allocate taskflow failed");
        free(tf);
        return NULL;
    }

    MTHPC_INIT_WORK(&task->work, "taskflow start", mthpc_task_worker, arg);
    task->parent = tf;
    task->func = start;
    mthpc_list_init(&task->list_node);
    mthpc_list_init(&task->internal_list_head);
    mthpc_list_init(&task->internal_list_node);

    mthpc_list_add(&tf->list_head, &task->list_node);

    return tf;
}

int mthpc_taskflow_await(struct mthpc_taskflow *tf)
{
    struct mthpc_task *current = NULL;

    mthpc_list_for_each_entry (current, &tf->list_head, list_node) {
        struct mthpc_task *internal_current = NULL;

        mthpc_completion_init(&tf->completion, current->nr_internal);

        mthpc_list_for_each_entry (internal_current, &current->internal_list_head, list_node)
            mthpc_queue_taskflow_work(&internal_current->work);

        mthpc_wait_for_completion(&tf->completion);
    }

    return 0;
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
