#ifndef __MTHPC_INTERNAL_WORKQUEUE_H__
#define __MTHPC_INTERNAL_WORKQUEUE_H__

struct mthpc_work;

/* Thread workqueue */

int mthpc_queue_thread_work(struct mthpc_work *work);

/* Taskflow workqueue */

int mthpc_queue_taskflow_work(struct mthpc_work *work);

#endif /* __MTHPC_INTERNAL_WORKQUEUE_H__ */
