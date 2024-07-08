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

void mthpc_safe_ptr_create(struct mthpc_safe_ptr *stack_sp, void *new_data,
                           void (*dtor)(void *));
void mthpc_safe_ptr_destroy(struct mthpc_safe_ptr *sp);
void mthpc_safe_ptr_store(struct mthpc_safe_ptr *sp, void *new_data,
                          void (*dtor)(void *));
void *mthpc_safe_ptr_load(struct mthpc_safe_ptr *sp);
int mthpc_safe_ptr_cmpxhg(struct mthpc_safe_ptr *sp, void **expected,
                          void *desired);
__allow_unused void mthpc_dump_safe_ptr(struct mthpc_safe_ptr *sp);

/* Declaration APIs */

#define MTHPC_DEFINE_SAFE_PTR(name, new_data, dtor)       \
    struct mthpc_safe_ptr name                            \
        __attribute__((cleanup(mthpc_safe_ptr_destroy))); \
    do {                                                  \
        mthpc_safe_ptr_create(&name, new_data, dtor);     \
    } while (0)

#define MTHPC_DECLARE_SAFE_PTR(name) MTHPC_DEFINE_SAFE_PTR(name, NULL, NULL)

/*
 * Borrow APIs
 *
 * We don't create the new safe_ptr, instead, we increase the refcount of
 * safe_ptr.
 */

#ifdef __CHECKER__
#define __mthpc_brw __attribute__((noderef, address_space(1)))
#define __mthpc_move __attribute__((noderef, address_space(2)))
#else
#define __mthpc_brw
#define __mthpc_move
#endif

struct mthpc_safe_ptr *
mthpc_pass_sp_post(struct mthpc_safe_ptr __mthpc_brw *brw_sp,
                   struct mthpc_safe_ptr *sp);

static inline struct mthpc_safe_ptr __mthpc_brw *
mthpc_borrow_safe_ptr(struct mthpc_safe_ptr *sp)
{
    return enchant_ptr(sp, __mthpc_brw);
}

#define MTHPC_DECLARE_SAFE_PTR_FROM_BORROW(name, brw_sp)  \
    struct mthpc_safe_ptr name                            \
        __attribute__((cleanup(mthpc_safe_ptr_destroy))); \
    do {                                                  \
        check_enchant_ptr(brw_sp, __mthpc_brw);           \
        mthpc_pass_sp_post(brw_sp, &name);                \
    } while (0)

static inline struct mthpc_safe_ptr __mthpc_move *
mthpc_move_safe_ptr(struct mthpc_safe_ptr *sp)
{
    return enchant_ptr(sp, __mthpc_move);
}

#define MTHPC_DECLARE_SAFE_PTR_FROM_MOVE(name, move_sp)   \
    struct mthpc_safe_ptr name                            \
        __attribute__((cleanup(mthpc_safe_ptr_destroy))); \
    do {                                                  \
        check_enchant_ptr(brw_sp, __mthpc_move);          \
        mthpc_pass_sp_post(move_sp, &name);               \
    } while (0)

#endif /* __MTHPC_SAFE_PTR_H__ */
