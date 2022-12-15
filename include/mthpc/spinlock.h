#ifndef __MTHPC_SPINLOCK_H__
#define __MTHPC_SPINLOCK_H__

#include <pthread.h>

#include <mthpc/util.h>
#include <mthpc/debug.h>

#ifndef spinlock_t
typedef pthread_mutex_t spinlock_t;
#endif

#ifndef DEFINE_SPINLOCK
#define DEFINE_SPINLOCK(lock) spinlock_t lock = PTHREAD_MUTEX_INITIALIZER
#endif

#ifndef SPINLOCK_INIT
#define SPINLOCK_INIT PTHREAD_MUTEX_INITIALIZER
#endif

#ifndef spin_lock_init
static __always_inline void spin_lock_init(spinlock_t *sp)
{
    int ret;

    ret = pthread_mutex_init(sp, NULL);
    MTHPC_BUG_ON(ret != 0, "spin_lock_init:pthread_mutex_init:%d\n", ret);
}
#endif

#ifndef spin_lock_destroy
static __always_inline void spin_lock_destroy(spinlock_t *sp)
{
    int ret;

    ret = pthread_mutex_destroy(sp);
    MTHPC_BUG_ON(ret != 0, "spin_lock_destroy:pthread_mutex_destroy:%d\n", ret);
}
#endif

#ifndef spin_lock
static __always_inline void spin_lock(spinlock_t *sp)
{
    int ret;

    ret = pthread_mutex_lock(sp);
    MTHPC_BUG_ON(ret != 0, "spin_lock:pthread_mutex_lock:%d\n", ret);
}
#endif

#ifndef spin_unlock
static __always_inline void spin_unlock(spinlock_t *sp)
{
    int ret;

    ret = pthread_mutex_unlock(sp);
    MTHPC_BUG_ON(ret != 0, "spin_unlock:pthread_mutex_unlock:%d\n", ret);
}
#endif

#endif /* __MTHPC_SPINLOCK_H__ */
