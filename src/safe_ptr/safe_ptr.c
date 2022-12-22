#include <stdlib.h>

#include <mthpc/safe_ptr.h>

#include <internal/feature.h>

#undef _MTHPC_FEATURE
#define _MTHPC_FEATURE safe_ptr

void *__mthpc_safe_alloc(const char *name, size_t size, void (*dtor)(void *))
{
    struct mthpc_safe_proto *sp = NULL;

    sp = malloc(sizeof(struct mthpc_safe_proto) + size);
    MTHPC_WARN_ON(!sp, "malloc");
    if (!sp)
        return NULL;

    MTHPC_SAFE_INIT(sp, name, dtor);

    return mthpc_safe_data_of(sp);
}

void __mthpc_safe_get(struct mthpc_safe_proto *sp)
{
    __atomic_fetch_add(&sp->refcount, 1, __ATOMIC_RELEASE);
}

void __mthpc_safe_put(struct mthpc_safe_proto *sp)
{
    if (!__atomic_add_fetch(&sp->refcount, -1, __ATOMIC_ACQUIRE)) {
        if (sp->dtor)
            sp->dtor(mthpc_safe_data_of(sp));
        free(sp);
    }
}

static __mthpc_init void mthpc_safe_ptr_init(void)
{
    mthpc_init_feature();
    mthpc_init_ok();
}

static __mthpc_exit void mthpc_safe_ptr_exit(void)
{
    mthpc_exit_feature();
    mthpc_exit_ok();
}
