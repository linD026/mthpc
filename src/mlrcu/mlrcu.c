#include <stdlib.h>
#include <thread.h>

struct mlrcu_layer {
    alignas(128) atomic_uint updated;
    alignas(128) void *data;
    unsigned int order;
    struct mlrcu_layer *next;
};

struct mlrcu_data {
    alignas(128) void *data;
};

static inline struct mlrcu_layer *mlrcu_layer_ctor(unsigned int order)
{
    struct mlrcu_layer *layer = malloc(sizeof(struct mlrcu_layer));
    if (!layer)
        return NULL;
    
    layer->local_lock = 0;
    layer->data = NULL;
    layer->order = order;
    layer->next = NULL;

    return layer;
}

static inline void *mlrcu_layer_dtor(struct mlrcu_layer *layer)
{
    if (!layer)
        return;

    free(layer);
}

void mlrcu_update(struct mlrcu_layer *layer, void *data)
{
    void *old_data = NULL;
    if (!layer)
        return;
    old_data = atomic_exchange_explicit(&layer->data, data, memory_order_relaxed);
    atomic_fetch_add_explicit(&layer->updated, 1, memory_order_relaxed);

    mlrcu_recycle(layer, old_data);
}

#include "centralized_barrier.h"

#define nr_thread 10

static DEFINE_BARRIER(th_barrier);

static int read_func(void *unused)
{
    barrier(th_barrier, nr_thread * 2);

    return 0;
}

static int write_func(void *unused)
{
    barrier(th_barrier, nr_thread * 2);

    return 0;
}

int main(void)
{
    thrd_t th_read[nr_thread];
    thrd_t th_write[nr_thread];

    for (int i = 0; i < nr_thread; i++)
        thrd_create(&th_read[i], read_func, NULL);
    for (int i = 0; i < nr_thread; i++)
        thrd_create(&th_write[i], write_func, NULL);

    for (int i = 0; i < nr_thread; i++)
        thrd_join(th_read[i], NULL);
    for (int i = 0; i < nr_thread; i++)
        thrd_join(&th_write[i], NULL);

}
