#include <stdlib.h>

#include <mthpc/safe_ptr.h>

#include <internal/feature.h>
#undef _MTHPC_FEATURE
#define _MTHPC_FEATURE safe_ptr

void *__mthpc_unsafe_alloc(const char *name, size_t size, void (*dtor)(void *))
{
    struct mthpc_safe_proto *sp = NULL;

    sp = malloc(sizeof(struct mthpc_safe_proto) + size);
    MTHPC_WARN_ON(!sp, "malloc");
    if (!sp)
        return NULL;

    MTHPC_SAFE_INIT(sp, name, dtor);

    return mthpc_safe_data_of(sp);
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
