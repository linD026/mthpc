#ifndef __MTHPC_SAFE_PTR_H__
#define __MTHPC_SAFE_PTR_H__

#include <stdatomic.h>

#include <mthpc/debug.h>

struct mthpc_rcu_node;

struct mthpc_safe_ptr {
    atomic_uintptr_t cb;
    unsigned long id;
    struct mthpc_rcu_node *rcu_node;
};

void mthpc_safe_ptr_create(struct mthpc_safe_ptr *stack_sp, size_t size,
                           void (*dtor)(void *));
void mthpc_safe_ptr_destroy(struct mthpc_safe_ptr *sp);
void mthpc_safe_ptr_drop(struct mthpc_safe_ptr *sp);
// Store new safe data
void mthpc_safe_ptr_store_sp(struct mthpc_safe_ptr *sp, void *new);
// Store the raw data
void __mthpc_safe_ptr_store_data(struct mthpc_safe_ptr *sp, void *raw_data,
                                 size_t size);
#define mthpc_safe_ptr_store_data(sp, raw_data) \
    __mthpc_safe_ptr_store_data(sp, raw_data, sizeof(__typeof__(raw_data)))
void mthpc_safe_ptr_load(struct mthpc_safe_ptr *sp, void *dst);
int mthpc_safe_ptr_cmpxhg(struct mthpc_safe_ptr *sp, void *expected,
                          void *desired);
__allow_unused void mthpc_dump_safe_ptr(struct mthpc_safe_ptr *sp);

/* Declaration APIs */

#define MTHPC_DECLARE_SAFE_PTR(type, name, dtor)          \
    struct mthpc_safe_ptr name                            \
        __attribute__((cleanup(mthpc_safe_ptr_destroy))); \
    do {                                                  \
        mthpc_safe_ptr_create(&name, sizeof(type), dtor); \
    } while (0)

/*
 * Borrow APIs
 *
 * We don't create the new safe_ptr, instead, we increase the refcount of
 * safe_ptr.
 */

#ifdef __CHECKER__
#define __mthpc_brw __attribute__((noderef, address_space(1)))
#else
#define __mthpc_brw
#endif

struct mthpc_safe_ptr __mthpc_brw *
mthpc_borrow_safe_ptr(struct mthpc_safe_ptr *sp)
{
    return enchant_ptr(sp, __mthpc_brw);
}

struct mthpc_safe_ptr *
mthpc_borrow_post(struct mthpc_safe_ptr __mthpc_brw *brw_sp,
                  struct mthpc_safe_ptr *sp);

#define MTHPC_DECLARE_SAFE_PTR_FROM_BORROW(type, name, brw_sp) \
    struct mthpc_safe_ptr name                                 \
        __attribute__((cleanup(mthpc_safe_ptr_destroy)));      \
    do {                                                       \
        mthpc_borrow_post(brw_sp, &name);                      \
    } while (0)

#endif /* __MTHPC_SAFE_PTR_H__ */
