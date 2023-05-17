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
    unsigned long __is_sub_task_parent;
    void (*func)(void *);
    /* Belongs to the taskflow->list_head. It's the main task list. */
    struct mthpc_list_head list_node;

    /* The number of all the task (including itself) */
    unsigned long nr_sub_task;
    /*
     * Currently, we only support the two hierarchy, task and sub task.
     * In other words, we don't have sub sub-task. It's the sub task list.
     */
    struct mthpc_list_head sub_task_list_head;
};

struct mthpc_taskflow {
    struct mthpc_list_head list_head;
    unsigned long nr_task; /* The total number of sub tasks */
    struct mthpc_completion completion;
} __attribute__((aligned(sizeof(unsigned long))));

static __always_inline int mthpc_is_sub_task(struct mthpc_task *task)
{
    return (int)task->__is_sub_task_parent & 0x1;
}

static __always_inline void mthpc_set_sub_task(struct mthpc_task *task)
{
    task->__is_sub_task_parent |= 0x1;
}

static __always_inline void mthpc_unset_sub_task(struct mthpc_task *task)
{
    task->__is_sub_task_parent &= ~0x1;
}

static __always_inline struct mthpc_taskflow *
mthpc_get_taskflow(struct mthpc_task *task)
{
    return (struct mthpc_taskflow *)(task->__is_sub_task_parent & (~0x1));
}

static __always_inline void mthpc_set_taskflow(struct mthpc_taskflow *tf,
                                               struct mthpc_task *task)
{
    task->__is_sub_task_parent =
        (unsigned long)tf | (unsigned long)mthpc_is_sub_task(task);
}

static void mthpc_task_worker(struct mthpc_work *work)
{
    struct mthpc_task *task = container_of(work, struct mthpc_task, work);
    struct mthpc_taskflow *tf = mthpc_get_taskflow(task);
    task->func(work->private);
    mthpc_complete(&tf->completion);
}

static struct mthpc_task *__mthpc_task_alloc(struct mthpc_taskflow *tf,
                                             const char *work_name,
                                             void (*func)(void *arg), void *arg)
{
    struct mthpc_task *task = malloc(sizeof(struct mthpc_task));
    if (!task) {
        MTHPC_WARN_ON(1, "allocate task failed");
        return NULL;
    }

    MTHPC_INIT_WORK(&task->work, work_name, mthpc_task_worker, arg);
    mthpc_set_taskflow(tf, task);
    task->func = func;
    task->nr_sub_task = 1;
    mthpc_list_init(&task->list_node);
    mthpc_list_init(&task->sub_task_list_head);

    /* We don't link the task here. see taskflow_task_create family. */
    tf->nr_task++;

    return task;
}

/*
 * Find the task's location, delete it in the orignal location,
 * mark it as sub-task and add to the @list.
 */
static void mthpc_taskflow_rlist_del(struct mthpc_list_head *list,
                                     struct mthpc_task **tasks, int nr_task)
{
    unsigned int i;

    /* We reuse the task->sub_task_list_head */
    mthpc_list_init(list);

    for (i = 0; i < nr_task; i++) {
        struct mthpc_task *tmp = tasks[i];

        if (mthpc_is_sub_task(tmp)) {
            struct mthpc_task *current;

            mthpc_list_del(&tmp->sub_task_list_head);
            // TODO: Find an efficient way to decrease the sub-task number
            mthpc_list_for_each_entry (current, &tmp->sub_task_list_head,
                                       sub_task_list_head) {
                if (!mthpc_is_sub_task(current)) {
                    current->nr_sub_task--;
                    break;
                }
            }
            mthpc_list_add(&tmp->sub_task_list_head, list);
        } else {
            /*
             * We are going to move the main task. Check if there are any
             * sub-task in the list.
             */
            if (tmp->nr_sub_task == 1) {
                /* only itself */
                mthpc_list_del(&tmp->list_node);
                mthpc_list_add_tail(&tmp->sub_task_list_head, list);
            } else {
                /* dup the main task info, and link it to the main list. */
                struct mthpc_task *next_head =
                    mthpc_list_next_entry(tmp, sub_task_list_head);

                next_head->nr_sub_task = tmp->nr_sub_task - 1;
                tmp->nr_sub_task = 1;

                /* Let's the next_head task become the main task of sub list. */
                mthpc_list_del(&tmp->sub_task_list_head);

                /* link to the main list */
                mthpc_list_add_tail(&next_head->sub_task_list_head, list);
            }
        }
        /* We set the task as sub-task type. */
        mthpc_set_sub_task(tmp);
    }
}

/* user API */

void __mthpc_taskflow_precede(struct mthpc_task *task, struct mthpc_task **news,
                              int nr_task)
{
    struct mthpc_taskflow *tf = mthpc_get_taskflow(task);
    struct mthpc_list_head adding_list;

    if (unlikely(!nr_task)) {
        MTHPC_WARN_ON(nr_task == 0, "nr_task is zero");
        return;
    }

    mthpc_taskflow_rlist_del(&adding_list, news, nr_task);

    /* If @task is the first main task, we need to create a new main task. */
    if (task->list_node.prev == &tf->list_head) {
        struct mthpc_task *new_main_task = container_of(
            adding_list.next, struct mthpc_task, sub_task_list_head);
        mthpc_unset_sub_task(new_main_task);
        new_main_task->nr_sub_task = nr_task;
        mthpc_list_splice_tail(&adding_list, &task->list_node);
    } else {
        /* Otherwise, we just add them to the prev main task's sub-list. */
        mthpc_list_splice_tail(&adding_list, &task->sub_task_list_head);
    }
}

void __mthpc_taskflow_succeed(struct mthpc_task *task, struct mthpc_task **news,
                              int nr_task)
{
    struct mthpc_taskflow *tf = mthpc_get_taskflow(task);
    struct mthpc_list_head adding_list;

    if (unlikely(!nr_task)) {
        MTHPC_WARN_ON(nr_task == 0, "nr_task is zero");
        return;
    }

    mthpc_taskflow_rlist_del(&adding_list, news, nr_task);

    /* If @task is the last main task, we need to create a new main task. */
    if (task->list_node.next == &tf->list_head) {
        struct mthpc_task *new_main_task = container_of(
            adding_list.next, struct mthpc_task, sub_task_list_head);
        mthpc_unset_sub_task(new_main_task);
        new_main_task->nr_sub_task = nr_task;
        mthpc_list_splice(&adding_list, &task->list_node);
    } else {
        /* Otherwise, we just add them to the next main task's sub-list. */
        mthpc_list_splice(&adding_list, &task->sub_task_list_head);
    }
}

struct mthpc_task *mthpc_task_create(struct mthpc_taskflow *tf,
                                     void (*func)(void *arg), void *arg)
{
    struct mthpc_task *task =
        __mthpc_task_alloc(tf, "taskflow task", func, arg);
    if (!task)
        return NULL;

    mthpc_unset_sub_task(task);
    mthpc_list_add_tail(&task->list_node, &tf->list_head);
    /* We increased the tf->nr_task in __mthpc_task_alloc already. */

    return task;
}

struct mthpc_task *mthpc_sub_task_create(struct mthpc_task *task,
                                         void (*func)(void *arg), void *arg)
{
    struct mthpc_task *sub_task = __mthpc_task_alloc(
        mthpc_get_taskflow(task), "taskflow sub task", func, arg);
    if (!task)
        return NULL;

    mthpc_set_sub_task(sub_task);
    mthpc_list_add_tail(&sub_task->sub_task_list_head,
                        &task->sub_task_list_head);
    /*
     * We increased the tf->nr_task in __mthpc_task_alloc already.
     * But we haven't increase the sub task number yet.
     */
    task->nr_sub_task++;

    return task;
}

struct mthpc_taskflow *mthpc_taskflow_create(void)
{
    struct mthpc_taskflow *tf = malloc(sizeof(struct mthpc_taskflow));
    if (!tf) {
        MTHPC_WARN_ON(1, "allocate taskflow failed");
        return NULL;
    }

    /*
     * We are using the LSB to set the sub-task flag, so once the LSB of
     * original address is non-zero bit, the behavior we get the taskflow
     * from task and the sub-task determination will go error.
     */
    MTHPC_BUG_ON(((unsigned long)tf & 0x1) == 0x1,
                 "The LSB of taskflow's address is not zero");

    mthpc_list_init(&tf->list_head);
    tf->nr_task = 0;
    mthpc_completion_init(&tf->completion, 0);

    return tf;
}

int mthpc_taskflow_await(struct mthpc_taskflow *tf)
{
    struct mthpc_task *current = NULL;
    /*
     * To verify the taskflow's task number is same as we completed,
     * we check it with nr_completed and nr_completed_sub_task.
     */
    unsigned long nr_completed = 0;

    mthpc_list_for_each_entry (current, &tf->list_head, list_node) {
        struct mthpc_task *sub_task_current = NULL;
        unsigned long nr_completed_sub_task = 0;

        mthpc_completion_init(&tf->completion, current->nr_sub_task);

        mthpc_queue_taskflow_work(&current->work);
        nr_completed_sub_task++;

        /*
         * Before we go to the next main task, complete the sub task list first.
         */
        mthpc_list_for_each_entry (sub_task_current,
                                   &current->sub_task_list_head,
                                   sub_task_list_head) {
            mthpc_queue_taskflow_work(&sub_task_current->work);
            nr_completed_sub_task++;
        }

        mthpc_wait_for_completion(&tf->completion);

        MTHPC_WARN_ON(
            nr_completed_sub_task != current->nr_sub_task,
            "the sub task number is unmatched (completed:%lu, sub_task:%lu)",
            nr_completed_sub_task, current->nr_sub_task);
        nr_completed += nr_completed_sub_task;
    }

    MTHPC_WARN_ON(nr_completed != tf->nr_task,
                  "the task number is unmatched (completed:%lu, task:%lu)",
                  nr_completed, tf->nr_task);

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
