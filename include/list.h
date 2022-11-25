#ifndef __MTHPC_LIST_H__
#define __MTHPC_LIST_H__

#include <stddef.h>
#include <stdbool.h>

struct mthpc_list_head {
    struct mthpc_list_head *next;
    struct mthpc_list_head *prev;
};

static inline void mthpc_list_init(struct mthpc_list_head *node)
{
    node->next = node;
    node->prev = node;
}

static inline bool mthpc_list_empty(struct mthpc_list_head *head)
{
    return head->next == head;
}

static inline void __mthpc_list_add(struct mthpc_list_head *new,
                                    struct mthpc_list_head *prev,
                                    struct mthpc_list_head *next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

static inline void mthpc_list_add(struct mthpc_list_head *new,
                                  struct mthpc_list_head *head)
{
    __mthpc_list_add(new, head, head->next);
}

static inline void mthpc_list_add_tail(struct mthpc_list_head *new,
                                       struct mthpc_list_head *head)
{
    __mthpc_list_add(new, head->prev, head);
}

static inline void __mthpc_list_del(struct mthpc_list_head *prev,
                                    struct mthpc_list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void mthpc_list_del(struct mthpc_list_head *node)
{
    __mthpc_list_del(node->prev, node->next);
    mthpc_list_init(node);
}

#define mthpc_list_for_each(n, head) \
    for (n = (head)->next; n != (head); n = n->next)

#define mthpc_list_for_each_from(pos, head) \
    for (; pos != (head); pos = pos->next)

#define mthpc_list_for_each_safe(pos, n, head)             \
    for (pos = (head)->next, n = pos->next; pos != (head); \
         pos = n, n = pos->next)

#endif /* __MTHPC_LIST_H__ */
