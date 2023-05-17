#include <stdio.h>

#include <mthpc/taskflow.h>
#include <mthpc/print.h>

#define NR_TASKS 8

static void dummy_task(void *id)
{
    mthpc_pr_info("ID:%d\n", *(int *)id);
    return;
}

int main (void)
{
    struct mthpc_taskflow *tf = mthpc_taskflow_create();
    int task_id[NR_TASKS] = { 0 };
    struct mthpc_task *tasks[NR_TASKS];
    unsigned int i;

    for (i = 0; i < NR_TASKS; i++) {
        task_id[i] = i;
        tasks[i] = mthpc_task_create(tf, dummy_task, &task_id[i]);
    }

    mthpc_taskflow_await(tf);

    return 0;
}
