#ifndef __MTHPC_SAFE_PTR_H__
#define __MTHPC_SAFE_PTR_H__

#include <stdatomic.h>

#include <mthpc/debug.h>

struct mthpc_safe_proto {
    const char *name;
    atomic_ulong refcount;
    unsigned int type;
    void (*dtor)(void *safe_data);
};

#define MTHPC_SAFE_INIT(sp, _name, _dtor) \
    do {                                  \
        (sp)->name = _name;               \
        (sp)->refcount = 0;               \
        (sp)->type = 0;                   \
        (sp)->dtor = _dtor;               \
    } while (0)

/*
 * copy; reset refcount
 */

static inline void *mthpc_safe_data_of(struct mthpc_safe_proto *sp)
{
    MTHPC_BUG_ON(!sp, "sp == NULL");
    return (void *)((char *)sp + sizeof(struct mthpc_safe_proto));
}

static inline struct mthpc_safe_proto *mthpc_safe_proto_of(void *safe_data)
{
    MTHPC_BUG_ON(!safe_data, "safe_data == NULL");
    return (struct mthpc_safe_proto *)((char *)safe_data -
                                       sizeof(struct mthpc_safe_proto));
}

void *__mthpc_unsafe_alloc(const char *name, size_t size, void (*dtor)(void *));

static inline void __mthpc_safe_get(struct mthpc_safe_proto *sp)
{
    atomic_fetch_add_explicit(&sp->refcount, 1, memory_order_release);
}

static inline int __mthpc_safe_put(struct mthpc_safe_proto *sp)
{
    int ret =
        atomic_fetch_add_explicit(&sp->refcount, -1, memory_order_seq_cst);
    if (ret - 1 == 0) {
        if (sp->dtor)
            sp->dtor(mthpc_safe_data_of(sp));
        free(sp);
    }
    return ret;
}

#define mthpc_unsafe_alloc(data_name, dtor) \
    __mthpc_unsafe_alloc(#data_name, sizeof(data_name), dtor)

#define mthpc_safe_get(safe_data) \
    __mthpc_safe_get(mthpc_safe_proto_of(safe_data))

#define mthpc_safe_put(safe_data)                                       \
    do {                                                                \
        int __s_p_t = __mthpc_safe_put(mthpc_safe_proto_of(safe_data)); \
        if (!__s_p_t)                                                   \
            __sci_##safe_data.sp = NULL;                                \
    } while (0)

struct mthpc_safe_cleanup_info {
    struct mthpc_safe_proto *sp;
};

static inline void mthpc_safe_cleanup(struct mthpc_safe_cleanup_info *sci)
{
    if (!sci->sp)
        return;
    __mthpc_safe_put(sci->sp);
}

#define MTHPC_DECLARE_SAFE_PTR(type, name, safe_data) \
    struct mthpc_safe_cleanup_info __sci_##name       \
        __attribute__((cleanup(mthpc_safe_cleanup))); \
    type *name = safe_data;                           \
    do {                                              \
        __sci_##name.sp = mthpc_safe_proto_of(name);  \
        mthpc_safe_get(name);                         \
    } while (0)

/* Declare the safe object and it will return pointer */
#define MTHPC_MAKE_SAFE_PTR(type, name, dtor) \
    MTHPC_DECLARE_SAFE_PTR(type, name, mthpc_unsafe_alloc(type, dtor));

struct mthpc_borrow_protected {
    struct mthpc_safe_proto *sp;
};

#define mthpc_borrow_to(safe_data)                     \
    ({                                                 \
        struct mthpc_borrow_protected __m_b_p_t;       \
        __m_b_p_t.sp = mthpc_safe_proto_of(safe_data); \
        &__m_b_p_t;                                    \
    })

#define mthpc_borrow_ptr(type) struct mthpc_borrow_protected *

#define MTHPC_DECLARE_SAFE_PTR_FROM_BORROW(type, name, borrow_data) \
    MTHPC_DECLARE_SAFE_PTR(type, name, mthpc_safe_data_of((borrow_data)->sp))

#endif /* __MTHPC_SAFE_PTR_H__ */
