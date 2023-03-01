#ifndef __MTHPC_SCOPED_LOCK_H__
#define __MTHPC_SCOPED_LOCK_H__

#include <stdatomic.h>

#ifndef ___PASTE
#define ___PASTE(a, b) a##b
#endif

#ifndef __PASTE
#define __PASTE(a, b) ___PASTE(a, b)
#endif

#ifndef __UNIQUE_ID
#define __UNIQUE_ID(prefix) __PASTE(__PASTE(__UNIQUE_ID_, prefix), __LINE__)
#endif

#define sl_spin_lock_type 0x0001
#define sl_rcu_read_lock_type 0x0002
#define sl_type_mask (sl_spin_lock_type | sl_rcu_read_lock_type)

struct mthpc_sl_node;

typedef struct mthpc_cached_scoped_lock {
    unsigned int type;
    unsigned int id;
    /* struct mthpc_sl_node *node; */
    atomic_uintptr_t node;
} mthpc_cached_scopedlock_t;

typedef struct mthpc_scoped_lock {
    unsigned int type;
    unsigned int id;
    struct mthpc_sl_node *node;
} mthpc_scopedlock_t;

void __mthpc_scoped_lock(mthpc_scopedlock_t *sl,
                         mthpc_cached_scopedlock_t *cached);
void mthpc_scoped_unlock(mthpc_scopedlock_t *sl);

static inline void
__mthpc_get_cached_scoped_lock(mthpc_scopedlock_t *sl,
                               mthpc_cached_scopedlock_t *cached)
{
    sl->node = (struct mthpc_sl_node *)atomic_load_explicit(
        &cached->node, memory_order_acquire);
}

#define __mthpc_cached_scoped_lock(_sl, __type)                           \
    static mthpc_cached_scopedlock_t _sl##_cached = { .node = 0 };        \
    mthpc_scopedlock_t _sl __attribute__((cleanup(mthpc_scoped_unlock))); \
    _sl.type = __type;                                                    \
    _sl.id = __LINE__;                                                    \
    _sl.node = NULL;                                                      \
    do {                                                                  \
        __mthpc_get_cached_scoped_lock(&_sl, &_sl##_cached);              \
        __mthpc_scoped_lock(&_sl, &_sl##_cached);                         \
    } while (0)

#define _intern_mthpc_scoped_lock(sl, type) __mthpc_cached_scoped_lock(sl, type)

#define mthpc_scoped_lock(__type) \
    _intern_mthpc_scoped_lock(__UNIQUE_ID(__type), sl_##__type##_type)

#endif /* __MTHPC_SCOPED_LOCK_H__ */
