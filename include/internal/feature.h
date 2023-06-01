#ifndef __MTHPC_INTERNAL_FEATURE_H__
#define __MTHPC_INTERNAL_FEATURE_H__

#include <mthpc/print.h>

#ifndef __stringfy
#define ___stringfy(n) #n
#define __stringfy(n) ___stringfy(n)
#endif

#define _MTHPC_FEATURE unkown
#define MTHPC_FEATURE _MTHPC_FEATURE
#define __MTHPC_FEATURE_NAME __stringfy(MTHPC_FEATURE)

enum mthpc_prio {
    mthpc_prio_base = 200,

    /* priority 1 - independent */
    mthpc_prio_rcu,
    mthpc_prio_centralized_barrier,
    mthpc_prio_safe_ptr,
    mthpc_prio_graph,

    /* priority 2 */
    mthpc_prio_thread, /* thread will use workqueue, so initialize first */

    /* priority 3 */
    mthpc_prio_workqueue,

    /* priority 4 */
    mthpc_prio_taskflow,

    mthpc_prio_nr,
};

#define ____mthpc_init(feature) \
    __attribute__((constructor(mthpc_prio_##feature)))
#define ___mthpc_init(feature) ____mthpc_init(feature)
#define __mthpc_init ___mthpc_init(_MTHPC_FEATURE)

#define ____mthpc_exit(feature) \
    __attribute__((destructor(mthpc_prio_##feature)))
#define ___mthpc_exit(feature) ____mthpc_exit(feature)
#define __mthpc_exit ___mthpc_exit(_MTHPC_FEATURE)

#define __mthpc_feature(name) mthpc_print(name " ...... ")

#define __mthpc_ok() mthpc_print("\e[32mok\e[0m\n")

#define mthpc_init_feature() \
    __mthpc_feature("[feature] " __MTHPC_FEATURE_NAME " init")
#define mthpc_exit_feature() \
    __mthpc_feature("[feature] " __MTHPC_FEATURE_NAME " exit")
#define mthpc_init_ok() __mthpc_ok()
#define mthpc_exit_ok() __mthpc_ok()

#endif /* __MTHPC_INTERNAL_FEATURE_H__ */
