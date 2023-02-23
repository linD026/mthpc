#ifndef __MTHPC_RCULIST_H__
#define __MTHPC_RCULIST_H__

#include <stddef.h>
#include <stdbool.h>

#include <mthpc/rcu.h>
#include <mthpc/util.h>
#include <mthpc/list.h>

#define mthpc_list_entry_rcu(ptr, type, member) \
    container_of(READ_ONCE(ptr), type, member)

#define mthpc_list_for_each_entry_rcu(pos, head, member)                     \
    for (pos = mthpc_list_entry_rcu((head)->next, __typeof__(*pos), member); \
         &pos->member != (head);                                             \
         pos =                                                               \
             mthpc_list_entry_rcu(pos->member.next, __typeof__(*pos), member))

static inline void __mthpc_list_add_rcu(struct mthpc_list_head *new,
                                        struct mthpc_list_head *prev,
                                        struct mthpc_list_head *next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    mthpc_cmb();
    mthpc_rcu_replace_pointer(prev->next, new);
}

static inline void mthpc_list_add_rcu(struct mthpc_list_head *new,
                                      struct mthpc_list_head *head)
{
    __mthpc_list_add_rcu(new, head, head->next);
}

static inline void mthpc_list_add_tail_rcu(struct mthpc_list_head *new,
                                           struct mthpc_list_head *head)
{
    __mthpc_list_add_rcu(new, head->prev, head);
}

static inline void __mthpc_list_del_rcu(struct mthpc_list_head *prev,
                                        struct mthpc_list_head *next)
{
    next->prev = prev;
    mthpc_cmb();
    WRITE_ONCE(prev->next, next);
}

static inline void mthpc_list_del_rcu(struct mthpc_list_head *node)
{
    __mthpc_list_del_rcu(node->prev, node->next);
    node->prev = NULL;
}

#endif /* __MTHPC_RCULIST_H__ */
