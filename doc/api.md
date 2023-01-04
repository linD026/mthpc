## Features

mthpc supports the following features.

- [Thread framework](#thread-framework)
- [Workqueue](#workqueue)
- [Centralized barrier](#centralized-barrier)
- [Read-Copy Update](#read-copy-update-rcu)
- [Scoped lock](#scoped-lock)
- [Safe pointer](#safe-pointer)

### Thread framework

```cpp
#include <mthpc/thread.h>
```

#### Declaration

Write your thread initialization and body functions with the following
prototype. And, delcare the thread object with the `MTHPC_DECLARE_THREAD` macro.

```cpp
void func(struct mthpc_thread *th);
MTHPC_DECLARE_THREAD(name, number_of_thread, init_func, body_func, args);
```

After that, with mutiple thread objects, you can also group those with the macro
, `MTHPC_DECLARE_THREADS`.

```cpp
MTHPC_DECLARE_THREADS(group_name, &threadA, &threadB, ...);
```

Note that all the object declarations are static size, which means they
**are not runtime-sized**.

#### APIs

Run the thread group with the blocking until all the threads finish.
Before execute the body function, all of the threads will wait until
all the initializations have been done.
Moreover, to let the object become async (non-blocking, auto-join) you
can use `_async_` function.

```cpp
void mthpc_thread_run(&object /* group or thread object */);
void mthpc_thread_async_run(&object /* group or thread object */);
void mthpc_thread_async_wait(&object /* group or thread object */);
```

#### Examples

* [thread self-test](../src/thread/test.c)
* [rcu self-test](../src/rcu/test.c)


### Workqueue

```cpp
#include <mthpc/workqueue.h>
```

#### Declaration

```cpp
void work_func(struct mthpc_work *work);
MTHPC_DECLARE_WORK(name, work_func, args);
```

#### APIs

To queue the work, use the following functions.

```cpp
int mthpc_schedule_work_on(int cpu, struct mthpc_work *work);
int mthpc_queue_work(struct mthpc_work *work);
```

You can also print out the information of the work.

```cpp
void mthpc_dump_work(struct mthpc_work *work)
```

#### Examples

* [workqueue self-test](../src/workqueue/test.c)


### Centralized barrier

```cpp
#include <mthpc/centralized_barrier.h>
```

#### Declaration

```cpp
MTHPC_DEFINE_BARRIER(name);
// or
struct mthpc_barrier name = MTHPC_BARRIER_INIT;
```

#### APIs

Add the barrier to the location you want with the number of the thread will
be blocked by this barrier.

```cpp
void mthpc_centralized_barrier(struct mthpc_barrier *b, size_t n);
```

###  Read-Copy Update (RCU)

```cpp
#include <mthpc/rcu.h>
#include <mthpc/rculist.h>
```

#### APIs

Before requiring the RCU read lock you can first initalize the RCU with
`void mthpc_rcu_thread_init(void)`.
In sometime, if you make sure that the thread will never use the RCU again,
you can call `void mthpc_rcu_thread_exit(void)` to release RCU resource of
that thread.

```cpp
void mthpc_rcu_read_lock(void);
void mthpc_rcu_read_unlock(void);

mthpc_rcu_replace_pointer(p, new);
mthpc_rcu_dereference(p);

void mthpc_synchronize_rcu(void);
void mthpc_synchronize_rcu_all(void);
```
#### Examples

* [rcu self-test](../src/rcu/test.c)

### Scoped lock

```cpp
#include <mthpc/scoped_lock.h>
```

#### APIs

```cpp
/* For lock_type, see following description. */
void mthpc_scoped_lock(lock_type);
```

Scoped lock support following lock types:
- `spin_lock`
- `rcu_read_lock`

#### Examples

* [scoped lock self-test](../src/scoped_lock/test.c)

### Safe pointer

```cpp
#include <mthpc/safe_ptr.h>
```

RAII type of object.
It is similar to the [`shared_ptr`](https://en.cppreference.com/w/cpp/memory/shared_ptr) in C++.

#### Declaration

```cpp
/* the object which protected by other safe_ptr or from mthpc_unsafe_alloc() */
MTHPC_DECLARE_SAFE_PTR(type, name, safe_data);
MTHPC_MAKE_SAFE_PTR(name, type, dtor);
```

#### APIs

```cpp
void *mthpc_unsafe_alloc(type, dtor);
void mthpc_safe_get(type *safe_ptr);
void mthpc_safe_put(type *safe_ptr);
```

To pass the safe data to another function, use the borrow methods.

```cpp
function(mthpc_borrow_to(safe_ptr));

void function(mthpc_borrow_ptr(type) borrow_ptr)
{
    MTHPC_DECLARE_SAFE_PTR_FROM_BORROW(type, name, borrow_ptr);

    ...
}
```

#### Examples

* [safe pointer self-test](../src/safe_ptr/test.c)

---

## Other features

```cpp
#include <mthpc/spinlock.h>
#include <mthpc/util.h>
#include <mthpc/list.h>
#include <mthpc/debug.h>
#include <mthpc/print.h>
```

---

## Future works

- hash table
- mlrcu
