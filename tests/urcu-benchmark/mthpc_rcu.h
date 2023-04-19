#pragma once

#include "../../include/mthpc/rcu.h"

#ifdef rcu_read_lock
#undef rcu_read_lock
#define rcu_read_lock mthpc_rcu_read_lock
#endif /* rcu_read_lock */

#ifdef rcu_read_unlock
#undef rcu_read_unlock
#define rcu_read_unlock mthpc_rcu_read_unlock
#endif /* rcu_read_unlock */

#ifdef rcu_dereference
#undef rcu_dereference
#define rcu_dereferencek mthpc_rcu_dereference
#endif /* rcu_dereference */

#ifdef rcu_register_thread
#undef rcu_register_thread
#define rcu_register_thread mthpc_rcu_thread_init
#endif /* rcu_register_thread */

#ifdef rcu_unregister_thread
#undef rcu_unregister_thread
#define rcu_unregister_thread mthpc_rcu_thread_exit
#endif /* rcu_unregister_thread */

#ifdef rcu_xchg_pointer
#undef rcu_xchg_pointer
#define rcu_xchg_pointer mthpc_rcu_replace_pointer
#endif /* rcu_xchg_pointer */

#ifdef synchronize_rcu
#undef synchronize_rcu
#define synchronize_rcu mthpc_synchronize_rcu
#endif /* synchronize_rcu */

