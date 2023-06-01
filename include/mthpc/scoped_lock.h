#ifndef __MTHPC_SCOPED_LOCK_H__
#define __MTHPC_SCOPED_LOCK_H__

#include <mthpc/guards.h>
#include <mthpc/rcu.h>
#include <mthpc/spinlock.h>

static inline void __mthpc_scoped_lock_rcu_read_lock(void *lock)
{
    mthpc_rcu_read_lock();
}

static inline void __mthpc_scoped_lock_rcu_read_unlock(void *lock)
{
    mthpc_rcu_read_unlock();
}

MTHPC_DECLARE_GUARD_OBJ(void, rcu, __mthpc_scoped_lock_rcu_read_lock,
                        __mthpc_scoped_lock_rcu_read_unlock);

MTHPC_DECLARE_GUARD_OBJ(spinlock_t, spinlock, spin_lock, spin_unlock);

#define mthpc_scoped_lock mthpc_guard

#endif /* __MTHPC_SCOPED_LOCK_H__ */
