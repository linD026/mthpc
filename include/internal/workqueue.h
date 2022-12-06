#ifndef __MTHPC_INTERNAL_WORKQUEUE_H__
#define __MTHPC_INTERNAL_WORKQUEUE_H__

struct mthpc_work;

/* Thread workqueue */

int mthpc_thread_work_on(struct mthpc_work *work);

#endif /* __MTHPC_INTERNAL_WORKQUEUE_H__ */
