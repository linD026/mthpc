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

#define MTHPC_TASKFLOW_CPU 0x0004
#define MTHPC_TASKFLOW_CPU_MASK (MTHPC_TASKFLOW_CPU - 1)

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
    struct mthpc_task *main_task;
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

static __allow_unused void mthpc_dump_task(struct mthpc_task *task)
{
    mthpc_pr_info("ID:%d, nr_task:%lu [prev=%p|node=%p|next=%p]\n",
                  *(int *)task->work.private, task->nr_sub_task,
                  task->list_node.prev, &task->list_node, task->list_node.next);
}

static void mthpc_task_worker(struct mthpc_work *work)
{
    struct mthpc_task *task = container_of(work, struct mthpc_task, work);
    struct mthpc_taskflow *tf = mthpc_get_taskflow(task);

    mthpc_dump_work(work);
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
    task->main_task = NULL;

    /* We don't link the task here. see taskflow_task_create family. */
    tf->nr_task++;

    return task;
}

/*
 * TODO: Provide more fine-grained control of moving the task.
 * For example. when we have the following list:
 *
 *  - A
 *  - B
 *    - C
 *    - D
 *    
 * If we do precede(C, D), we will move the D to the sub-list of B.
 * However, it should be like this:
 *
 * - A
 * - B
 *   - D
 *   - C
 *
 * Which means that we should make the main task as the sub-taskflow.
 */

/*
 * Find the task's location, delete it in the orignal location,
 * mark it as sub-task and add to the @list.
 */
static void mthpc_taskflow_rlist_del(struct mthpc_list_head *list,
                                     struct mthpc_task **tasks, int nr_task)
{
    unsigned int i;

    /* reuse sub_task_list_head at first. */
    mthpc_list_init(list);

    for (i = 0; i < nr_task; i++) {
        struct mthpc_task *tmp = tasks[i];

        if (mthpc_is_sub_task(tmp)) {
            mthpc_list_del(&tmp->sub_task_list_head);
            tmp->main_task->nr_sub_task--;
            tmp->main_task = NULL;
            mthpc_list_add(&tmp->sub_task_list_head, list);
        } else {
            MTHPC_WARN_ON(tmp->main_task,
                          "We should clear the main task's main_task");
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
                mthpc_list_replace_init(&tmp->list_node, &next_head->list_node);

                mthpc_list_add(&tmp->sub_task_list_head, list);
            }
        }
        /* We set the task as sub-task type. */
        mthpc_set_sub_task(tmp);
    }
}

static __allow_unused void mthpc_dump_taskflow(struct mthpc_taskflow *tf)
{
    struct mthpc_task *current = NULL;

    mthpc_print("=========[prev=%p|nr_task=%lu, list=%p|next=%p]============\n",
                tf->list_head.prev, tf->nr_task, &tf->list_head,
                tf->list_head.next);

    mthpc_list_for_each_entry (current, &tf->list_head, list_node) {
        struct mthpc_task *sub_task_current = NULL;

        mthpc_pr_info("ID:%d, nr_task:%lu [prev=%p|node=%p|next=%p]\n",
                      *(int *)current->work.private, current->nr_sub_task,
                      current->list_node.prev, &current->list_node,
                      current->list_node.next);

        mthpc_list_for_each_entry (sub_task_current,
                                   &current->sub_task_list_head,
                                   sub_task_list_head) {
            mthpc_pr_info("    ID:%d, nr_task:%lu [prev=%p|node=%p|next=%p]\n",
                          *(int *)sub_task_current->work.private,
                          sub_task_current->nr_sub_task,
                          sub_task_current->sub_task_list_head.prev,
                          &sub_task_current->sub_task_list_head,
                          sub_task_current->sub_task_list_head.next);
        }
    }
    mthpc_print("=======================================================\n");
}

static __always_inline int mthpc_taskflow_get_cpu(unsigned long seed)
{
    return (int)seed & MTHPC_TASKFLOW_CPU_MASK;
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

    mthpc_dump_taskflow(tf);
    if (mthpc_is_sub_task(task))
        task = task->main_task;

    mthpc_taskflow_rlist_del(&adding_list, news, nr_task);

    /* If @task is the first main task, we need to create a new main task. */
    if (task->list_node.prev == &tf->list_head) {
        struct mthpc_task *new_main_task;
        struct mthpc_list_head new_adding_list;
        struct mthpc_list_head *pos, *n;

        new_main_task = container_of(adding_list.next, struct mthpc_task,
                                     sub_task_list_head);
        mthpc_unset_sub_task(new_main_task);
        new_main_task->nr_sub_task = nr_task;

        /* Re-link the list to list_node. */
        mthpc_list_init(&new_adding_list);
        mthpc_list_for_each_safe (pos, n, &adding_list) {
            struct mthpc_task *tmp =
                container_of(pos, struct mthpc_task, sub_task_list_head);
            mthpc_list_del(&tmp->sub_task_list_head);
            mthpc_list_add_tail(&tmp->list_node, &new_adding_list);
        }

        mthpc_list_splice_tail(&new_adding_list, &task->list_node);
    } else {
        /* Otherwise, we just add them to the prev main task's sub-list. */
        struct mthpc_task *prev_task = mthpc_list_prev_entry(task, list_node);
        struct mthpc_task *curr = NULL;

        /* Otherwise, we just add them to the next main task's sub-list. */

        /* Link the to main task */
        mthpc_list_for_each_entry (curr, &adding_list, sub_task_list_head)
            curr->main_task = prev_task;

        /* Add to main task's sub task. */
        mthpc_list_splice_tail(&adding_list, &prev_task->sub_task_list_head);
        prev_task->nr_sub_task += nr_task;
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

    mthpc_dump_taskflow(tf);
    if (mthpc_is_sub_task(task))
        task = task->main_task;

    mthpc_taskflow_rlist_del(&adding_list, news, nr_task);

    /* If @task is the last main task, we need to create a new main task. */
    if (task->list_node.next == &tf->list_head) {
        struct mthpc_task *new_main_task;
        struct mthpc_list_head new_adding_list;
        struct mthpc_list_head *pos, *n;

        new_main_task = container_of(adding_list.next, struct mthpc_task,
                                     sub_task_list_head);
        mthpc_unset_sub_task(new_main_task);
        new_main_task->nr_sub_task = nr_task;

        /* Re-link the list to list_node. */
        mthpc_list_init(&new_adding_list);
        mthpc_list_for_each_safe (pos, n, &adding_list) {
            struct mthpc_task *tmp =
                container_of(pos, struct mthpc_task, sub_task_list_head);
            mthpc_list_del(&tmp->sub_task_list_head);
            mthpc_list_add_tail(&tmp->list_node, &new_adding_list);
        }

        mthpc_list_splice(&new_adding_list, &task->list_node);
    } else {
        struct mthpc_task *next_task = mthpc_list_next_entry(task, list_node);
        struct mthpc_task *curr = NULL;

        /* Otherwise, we just add them to the next main task's sub-list. */

        /* Link the to main task */
        mthpc_list_for_each_entry (curr, &adding_list, sub_task_list_head)
            curr->main_task = next_task;

        /* Add to main task's sub task. */
        mthpc_list_splice(&adding_list, &next_task->sub_task_list_head);
        next_task->nr_sub_task += nr_task;
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
    sub_task->main_task = task;
    /*
     * We increased the tf->nr_task in __mthpc_task_alloc already.
     * But we haven't increase the sub task number yet.
     */
    task->nr_sub_task++;

    return sub_task;
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

    mthpc_dump_taskflow(tf);

    mthpc_list_for_each_entry (current, &tf->list_head, list_node) {
        struct mthpc_task *sub_task_current = NULL;
        unsigned long nr_completed_sub_task = 0;

        mthpc_completion_init(&tf->completion, current->nr_sub_task);

        mthpc_schedule_taskflow_work_on(
            mthpc_taskflow_get_cpu(nr_completed_sub_task), &current->work);
        nr_completed_sub_task++;

        /*
         * Before we go to the next main task, complete the sub task list first.
         */
        mthpc_list_for_each_entry (sub_task_current,
                                   &current->sub_task_list_head,
                                   sub_task_list_head) {
            mthpc_schedule_taskflow_work_on(
                mthpc_taskflow_get_cpu(nr_completed_sub_task),
                &sub_task_current->work);
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
