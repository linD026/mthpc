#ifndef __MTHPC_LIST_H__
#define __MTHPC_LIST_H__

#include <stddef.h>
#include <stdbool.h>

#ifndef container_of
#define container_of(ptr, type, member)                        \
    __extension__({                                            \
        const __typeof__(((type *)0)->member) *__mptr = (ptr); \
        (type *)((char *)__mptr - offsetof(type, member));     \
    })
#endif

#define mthpc_list_entry(ptr, type, member) container_of(ptr, type, member)

struct mthpc_list_head {
    struct mthpc_list_head *next;
    struct mthpc_list_head *prev;
};

static inline void mthpc_list_init(struct mthpc_list_head *node)
{
    node->next = node;
    node->prev = node;
}

static inline bool mthpc_list_empty(const struct mthpc_list_head *head)
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

static inline void mthpc_list_replace(struct mthpc_list_head *old,
                                      struct mthpc_list_head *new)
{
    new->next = old->next;
    new->next->prev = new;
    new->prev = old->prev;
    new->prev->next = new;
}

static inline void mthpc_list_replace_init(struct mthpc_list_head *old,
                                           struct mthpc_list_head *new)
{
    mthpc_list_replace(old, new);
    mthpc_list_init(old);
}

static inline void mthpc_list_move(struct mthpc_list_head *list,
                                   struct mthpc_list_head *head)
{
    __mthpc_list_del(list->prev, list->next);
    mthpc_list_add(list, head);
}

static inline void __mthpc_list_splice(const struct mthpc_list_head *list,
                                       struct mthpc_list_head *prev,
                                       struct mthpc_list_head *next)
{
    struct mthpc_list_head *first = list->next;
    struct mthpc_list_head *last = list->prev;

    first->prev = prev;
    prev->next = first;

    last->next = next;
    next->prev = last;
}

static inline void mthpc_list_splice(const struct mthpc_list_head *list,
                                     struct mthpc_list_head *head)
{
    if (!mthpc_list_empty(list))
        __mthpc_list_splice(list, head, head->next);
}

static inline void mthpc_list_splice_tail(struct mthpc_list_head *list,
                                          struct mthpc_list_head *head)
{
    if (!mthpc_list_empty(list))
        __mthpc_list_splice(list, head->prev, head);
}

#define mthpc_list_prev_entry(pos, member) \
    mthpc_list_entry((pos)->member.prev, __typeof__(*(pos)), member)

#define mthpc_list_next_entry(pos, member) \
    mthpc_list_entry((pos)->member.next, __typeof__(*(pos)), member)

#define mthpc_list_for_each(n, head) \
    for (n = (head)->next; n != (head); n = n->next)

#define mthpc_list_for_each_from(pos, head) \
    for (; pos != (head); pos = pos->next)

#define mthpc_list_for_each_safe(pos, n, head)             \
    for (pos = (head)->next, n = pos->next; pos != (head); \
         pos = n, n = pos->next)

#define mthpc_list_for_each_entry(pos, head, member)                     \
    for (pos = mthpc_list_entry((head)->next, __typeof__(*pos), member); \
         &pos->member != (head); pos = mthpc_list_next_entry(pos, member))

#endif /* __MTHPC_LIST_H__ */
