#ifndef __MTHPC_TASKFLOW_H__
#define __MTHPC_TASKFLOW_H__

#include <mthpc/util.h>

struct mthpc_task;

struct mthpc_taskflow {
    struct mthpc_task *start;
    int nr_task;
};

void __mthpc_taskflow_precede(struct mthpc_task *task, struct mthpc_task *tasks,
                              int nr_task);

#define mthpc_taskflow_precede(_task, ...)                                    \
    do {                                                                      \
        struct mthpc_task __tf_p_tasks[macro_var_args_count(__VA_ARGS__)] = { \
            __VA_ARGS__                                                       \
        };                                                                    \
        int __tf_p_nr_task = macro_var_args_count(__VA_ARGS__);               \
        __mthpc_taskflow_precede(_task, __tf_p_tasks, __tf_p_nr_task);        \
    } while (0)

void __mthpc_taskflow_succeed(struct mthpc_task *task, struct mthpc_task *tasks,
                              int nr_task);

#define mthpc_taskflow_succeed(_task, ...)                                    \
    do {                                                                      \
        struct mthpc_task __tf_s_tasks[macro_var_args_count(__VA_ARGS__)] = { \
            __VA_ARGS__                                                       \
        };                                                                    \
        int __tf_s_nr_task = macro_var_args_count(__VA_ARGS__);               \
        __mthpc_taskflow_succeed(_task, __tf_s_tasks, __tf_s_nr_task);        \
    } while (0)

struct mthpc_taskflow *mthpc_taskflow_create(void);

#endif /* __MTHPC_TASKFLOW_H__ */
