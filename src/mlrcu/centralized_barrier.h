/* 
 * barrier: centralized blocking barrier
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 * 
 * Copyright (C) 2022 linD026
 */

#ifndef __CENTRALIZED_BARRIER_H__
#define __CENTRALIZED_BARRIER_H__

#include <pthread.h>

#define __CB_ARCH_COHPAD 128 // x86 cacheline size
#define CB_COHPAD __CB_ARCH_COHPAD

struct barrier {
    int flag;
    int count;
    pthread_mutex_t lock;
};
__attribute__((aligned(CB_COHPAD)))

static __thread int local_sense = 0;

#define BARRIER_INIT                                             \
    {                                                            \
        .flag = 0, .count = 0, .lock = PTHREAD_MUTEX_INITIALIZER \
    }

#define DEFINE_BARRIER(name) struct barrier name = BARRIER_INIT

static inline void barrier(struct barrier *b, size_t n)
{
    local_sense = !local_sense;

    pthread_mutex_lock(&b->lock);
    b->count++;
    if (b->count == n) {
        b->count = 0;
        pthread_mutex_unlock(&b->lock);
        __atomic_store_n(&b->flag, local_sense, __ATOMIC_RELEASE);
    } else {
        pthread_mutex_unlock(&b->lock);
        while (__atomic_load_n(&b->flag, __ATOMIC_ACQUIRE) != local_sense)
            ;
    }
}

#endif /* __CENTRALIZED_BARRIER_H__ */
