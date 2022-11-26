#ifndef __MTHPC_RCU_H__
#define __MTHPC_RCU_H__

#include <pthread.h>

void mthpc_rcu_thread_init(void);

void mthpc_rcu_read_lock(void);
void mthpc_rcu_read_unlock(void);
void mthpc_synchronize_rcu(void);

#define mthpc_rcu_replace_pointer(p, new) \
    ({ __atomic_exchange_n(&(p), (new), __ATOMIC_RELEASE); })

void mthpc_synchronize_rcu_all(void);

#endif /* __MTHPC_RCU_H__ */
