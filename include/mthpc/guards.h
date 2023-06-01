#ifndef __MTHPC_GUARDS_H__
#define __MTHPC_GUARDS_H__

#include <mthpc/rcu.h>
#include <mthpc/spinlock.h>

#ifndef ___PASTE
#define ___PASTE(a, b) a##b
#endif

#ifndef __PASTE
#define __PASTE(a, b) ___PASTE(a, b)
#endif

#ifndef __UNIQUE_ID
#define __UNIQUE_ID(prefix) __PASTE(__PASTE(__UNIQUE_ID_, prefix), __LINE__)
#endif

#ifndef __allow_unused
#define __allow_unused __attribute__((unused))
#endif

#define __MTHPC_CLEANUP__(func) __attribute__((cleanup(func)))

#define MTHPC_DECLARE_GUARD_OBJ(_type, name, init, exit)                    \
    typedef struct {                                                        \
        _type *lock;                                                        \
    } mthpc_generated_guards_##name##_t;                                    \
    static inline mthpc_generated_guards_##name##_t __allow_unused          \
        mthpc_generated_guards_##name##_init(_type *lock)                   \
    {                                                                       \
        mthpc_generated_guards_##name##_t _G = { .lock = lock };            \
        init(_G.lock);                                                      \
        return _G;                                                          \
    }                                                                       \
    static inline void __allow_unused mthpc_generated_guards_##name##_exit( \
        void *__G)                                                          \
    {                                                                       \
        mthpc_generated_guards_##name##_t *_G =                             \
            (mthpc_generated_guards_##name##_t *)__G;                       \
        exit(_G->lock);                                                     \
    }

#define mthpc_guard(type, ...) __mthpc_guard(type, NULL, ##__VA_ARGS__)
#define __mthpc_guard(type, ...)                                          \
    MTHPC_DECLARE_LOCK_GUARD_ARGS(__VA_ARGS__, MTHPC_DECLARE_LOCK_GUARD1, \
                                  MTHPC_DECLARE_LOCK_GUARD0)              \
    (type, ##__VA_ARGS__)

#define MTHPC_DECLARE_LOCK_GUARD_ARGS(_DGN, _DG0, DECLARE_GUARD, ...) \
    DECLARE_GUARD

#define MTHPC_DECLARE_LOCK_GUARD0(type, ...)                                 \
    mthpc_generated_guards_##type##_t __UNIQUE_ID(type)                      \
    __allow_unused __MTHPC_CLEANUP__(mthpc_generated_guards_##type##_exit) = \
        mthpc_generated_guards_##type##_init(NULL)

#define MTHPC_DECLARE_LOCK_GUARD1(type, unused, lock_ptr)                    \
    mthpc_generated_guards_##type##_t __UNIQUE_ID(type)                      \
    __allow_unused __MTHPC_CLEANUP__(mthpc_generated_guards_##type##_exit) = \
        mthpc_generated_guards_##type##_init(lock_ptr)

static inline void __mthpc_scoped_lock_rcu_read_lock(void *lock)
{
    mthpc_rcu_read_lock();
}

static inline void __mthpc_scoped_lock_rcu_read_unlock(void *lock)
{
    mthpc_rcu_read_unlock();
}

MTHPC_DECLARE_GUARD_OBJ(void, rcu, __mthpc_scoped_lock_rcu_read_lock,
                        __mthpc_scoped_lock_rcu_read_unlock);

MTHPC_DECLARE_GUARD_OBJ(spinlock_t, spinlock, spin_lock, spin_unlock);

#endif /* __MTHPC_GUARDS_H__ */
