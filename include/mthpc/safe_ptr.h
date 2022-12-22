#ifndef __MTHPC_SAFE_PTR_H__
#define __MTHPC_SAFE_PTR_H__

#include <mthpc/debug.h>

struct mthpc_safe_proto {
    const char *name;
    unsigned long refcount;
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
 *  WARP(pointer)
 *
 * borrowing
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

void *__mthpc_safe_alloc(const char *name, size_t size, void (*dtor)(void *));

void __mthpc_safe_get(struct mthpc_safe_proto *sp);

void __mthpc_safe_put(struct mthpc_safe_proto *sp);

#define mthpc_safe_alloc(data_name, dtor) \
    __mthpc_safe_alloc(#data_name, sizeof(data_name), dtor)

#define mthpc_safe_get(safe_data) \
    __mthpc_safe_get(mthpc_safe_proto_of(safe_data))

#define mthpc_safe_put(safe_data) \
    __mthpc_safe_put(mthpc_safe_proto_of(safe_data))

struct mthpc_safe_cleanup_info {
    struct mthpc_safe_proto *sp;
};

static inline void mthpc_safe_cleanup(struct mthpc_safe_cleanup_info *sci)
{
    if (!sci->sp)
        return;
    __mthpc_safe_put(sci->sp);
}

#ifdef __CHECKER__
#define __mthpc_borrowed \
    __attribute__((noderef, address_space(__mthpc_borrowed)))
#define __mthpc_force __attribute__((force))
#else
#define __mthpc_borrowed
#define __mthpc_force
#endif /* __CHECKER__ */

#define MTHPC_DECLARE_SAFE_PTR(type, name, safe_data) \
    struct mthpc_safe_cleanup_info __sci_##name       \
        __attribute__((cleanup(mthpc_safe_cleanup))); \
    type *name = (type __mthpc_force *)safe_data;     \
    do {                                              \
        __sci_##name.sp = mthpc_safe_proto_of(name);  \
        mthpc_safe_get(name);                         \
    } while (0)

/* Declare the safe object and it will return pointer */
#define MTHPC_DECLARE_SAFE_DATA(type, name, dtor) \
    MTHPC_DECLARE_SAFE_PTR(type, name, mthpc_safe_alloc(type, dtor));

#define mthpc_borrow(safe_data) \
    ({ (typeof(*safe_data) __mthpc_force __mthpc_borrowed *) safe_data; })

#endif /* __MTHPC_SAFE_PTR_H__ */
