#ifndef __MTHPC_RCU_H__
#define __MTHPC_RCU_H__

#include <pthread.h>

void mthpc_rcu_thread_init(void);

inline void mthpc_rcu_read_lock(void);
inline void mthpc_rcu_read_unlock(void);
void mthpc_synchronize_rcu(void);

void mthpc_synchronize_rcu_all(void);

#endif /* __MTHPC_RCU_H__ */
