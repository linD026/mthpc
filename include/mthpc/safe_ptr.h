#ifndef __MTHPC_SAFE_PTR_H__
#define __MTHPC_SAFE_PTR_H__

#include <stdatomic.h>

#include <mthpc/debug.h>

struct mthpc_safe_cb;
struct mthpc_rcu_node;

struct mthpc_safe_ptr {
    struct mthpc_safe_cb *p;
    struct mthpc_rcu_node *rcu_node;
};

void mthpc_safe_ptr_create(struct mthpc_safe_ptr *stack_sp, size_t size,
                           void (*dtor)(void *));
void mthpc_safe_ptr_drop(struct mthpc_safe_ptr *sp);
void mthpc_safe_ptr_store(struct mthpc_safe_ptr *sp, void *new);
void *mthpc_safe_ptr_load(struct mthpc_safe_ptr *sp);
void *mthpc_safe_ptr_cmpxhg(struct mthpc_safe_ptr *sp, void *new);

/* Declaration APIs */

#define MTHPC_DEFINE_SAFE_PTR(type, name, dtor)                          \
    struct mthpc_safe_ptr name __attribute__((cleanup(mthpc_safe_put))); \
    do {                                                                 \
        mthpc_safe_ptr_create(name, typeof(type), dtor);                 \
    } while (0)

/*
 * Borrow APIs
 *
 * We don't create the new safe_ptr, instead, we increase the refcount of
 * safe_ptr.
 */

#define __mthpc_brw __attribute__((noderef, address_space(1)))

struct mthpc_safe_ptr __mthpc_brw *mthpc_borrow_to(struct mthpc_safe_ptr *sp);
void mthpc_borrow_post(struct mthpc_safe_ptr __mthpc_brw *brw_sp);

#define mthpc_borrow_ptr(type) struct mthpc_safe_ptr __mthpc_brw *

#define MTHPC_DECLARE_SAFE_PTR_FROM_BORROW(type, name, brw_sp)               \
    struct mthpc_safe_ptr name __attribute__((cleanup(mthpc_safe_put)));     \
    do {                                                                     \
        struct mthpc_safe_ptr *__t_brw_sp = (struct mthpc_safe_ptr *)brw_sp; \
        name.p = __t_brw_sp->p;                                              \
        name.rcu_node = __t_brw_sp->rcu_node;                                \
    } while (0)

#endif /* __MTHPC_SAFE_PTR_H__ */
