## Features

mthpc supports the following features.

- [Thread framework](#thread-framework)
- [Workqueue](#workqueue)
- [Centralized barrier](#centralized-barrier)
- [Wait for completion](#wait-for-completion)
- [Read-Copy Update](#read-copy-update-rcu)
- [Scoped lock](#scoped-lock)
- [Safe pointer](#safe-pointer)
- [Taskflow](#taskflow)

### Thread framework

```cpp
#include <mthpc/thread.h>
```

#### Declaration

Write your thread initialization and body functions with the following prototype
. And, delcare the thread group with the `MTHPC_DECLARE_THREAD_GROUP` macro.

```cpp
void func(struct mthpc_thread_group *th);
MTHPC_DECLARE_THREAD_GROUP(name, number_of_thread, init_func, body_func, args);
```

After that, with mutiple thread groups, you can also put it all together with
the macro, `MTHPC_DECLARE_THREAD_CLUSTER`.

```cpp
MTHPC_DECLARE_THREAD_CLUSTER(name, &groupA, &groupB, ...);
```

Note that all the object declarations are static size, which means they
**are not runtime-sized**.

#### APIs

Run the thread group/cluster with the blocking until all the threads finished.
Before executing the body function, all of the threads will wait until
all the initializations have been done. This kind of synchronization is
cluster-level.
Moreover, to let the object become async (non-blocking, auto-join) you
can use `_async_` function.

```cpp
void mthpc_thread_run(&threads /* group or cluster */);
void mthpc_thread_async_run(&threads /* group or cluster */);
void mthpc_thread_async_wait(&threads /* group or cluster */);
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
MTHPC_INIT_WORK(struct mthpc_work *work, name, work_func, private);
int mthpc_schedule_work_on(int cpu, struct mthpc_work *work);
int mthpc_queue_work(struct mthpc_work *work);
```

You can also print out the information of the work.

```cpp
void mthpc_dump_work(struct mthpc_work *work)
```

#### Examples

* [workqueue self-test](../src/workqueue/test.c)
* [Function-grained Task Control](https://github.com/linD026/Function-grained-Task-Control)


### Centralized barrier

```cpp
#include <mthpc/centralized_barrier.h>
```

#### Declaration

```cpp
MTHPC_DEFINE_BARRIER(name);
struct mthpc_barrier name = MTHPC_BARRIER_INIT;
```

#### APIs

Add the barrier to the location you want with the number of the thread will
be blocked by this barrier.

```cpp
void mthpc_barrier_init(struct mthpc_barrier *b);
void mthpc_centralized_barrier(struct mthpc_barrier *b, size_t n);
```

### Wait for completion

```cpp
#include <mthpc/completion.h>
```

#### Declaration

```cpp
MTHPC_DECLARE_COMPLETION(name, nr);
struct completion name = MTHPC_COMPLETION_INIT(nr);
```

#### APIs

```cpp

void mthpc_completion_init(struct mthpc_completion *completion,
                           unsigned long nr);
void mthpc_complete(struct mthpc_completion *completion);
void mthpc_wait_for_completion(struct mthpc_completion *completion);
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
/* For lock_type, see the following description. */
void mthpc_scoped_lock(lock_type);
void mthpc_scoped_lock(lock_type, lock_ptr);
```

Scoped lock support following lock types:
- `spinlock`
- `rcu`

#### Examples

* [scoped lock self-test](../src/scoped_lock/test.c)

### Safe pointer

```cpp
#include <mthpc/safe_ptr.h>
```

Atomic RAII type of object.
It is similar to the [`shared_ptr`](https://en.cppreference.com/w/cpp/memory/shared_ptr) in C++.

#### Declaration

```cpp
/* the object which protected by other safe_ptr or from mthpc_unsafe_alloc() */
MTHPC_DECLARE_SAFE_PTR(type, name, dtor);
MTHPC_DECLARE_SAFE_PTR_FROM_BORROW(type, name, struct mthpc_safe_ptr __mthpc_brw *brw_sp);
MTHPC_DECLARE_SAFE_PTR_FROM_MOVE(type, name, struct mthpc_safe_ptr __mthpc_move *move_sp);
```

#### APIs

```cpp
/* safe pointer operations */
void mthpc_safe_ptr_store_sp(struct mthpc_safe_ptr *sp, void *new);
void mthpc_safe_ptr_store_data(sp, raw_data);
void mthpc_safe_ptr_load(struct mthpc_safe_ptr *sp, void *dst);
int mthpc_safe_ptr_cmpxhg(struct mthpc_safe_ptr *sp, void *expected,
                          void *desired);

/* Debug */
__allow_unused void mthpc_dump_safe_ptr(struct mthpc_safe_ptr *sp);
```

To borrow the safe data to another function, use the borrow methods.
Every borrowed safe pointer has the `__mthpc_brw` attribute. This means
that the user cannot directly derefernce the borrowed safe pointer.

```cpp
/* borrow the safe pointer to other function. */
function(mthpc_borrow_safe_ptr(safe_ptr));

void function(struct mthpc_safe_ptr __mthpc_brw borrow_ptr)
{
    MTHPC_DECLARE_SAFE_PTR_FROM_BORROW(type, name, borrow_ptr);

    ...
}
```

However, if you want to transfer the ownership of safe pointer to other
function, use the move methods.

```cpp
function(mthpc_move_safe_ptr(safe_ptr));

void function(struct mthpc_safe_ptr __mthpc_move move_ptr)
{
    MTHPC_DECLARE_SAFE_PTR_FROM_BORROW(type, name, borrow_ptr);

    ...
}

```

#### Examples

* [safe pointer self-test](../src/safe_ptr/test.c)

### Taskflow
 
```cpp
#include <mthpc/taskflow.h>
```

#### Declaration

Use `mthpc_taskflow_create()` to create the task framework. And, use
`mthpc_task_create()` to add the new task.

```cpp
struct mthpc_taskflow *mthpc_taskflow_create(void);
struct mthpc_task *mthpc_task_create(struct mthpc_taskflow *tf,
                                     void (*func)(void *arg), void *arg);

struct mthpc_task *mthpc_sub_task_create(struct mthpc_task *task,
                                         void (*func)(void *arg), void *arg);
```

#### APIs

Use `precede()` and `succeed()` functions to decide which task run first.
After that, use `await()` to run all the tasks in the framework.

```cpp
void mthpc_taskflow_precede(task, forward_tasks...);
void mthpc_taskflow_succeed(task, backward_tasks...);
int mthpc_taskflow_await(struct mthpc_taskflow *tf);
```

#### Examples

* [taskflow self-test](../src/taskflow/draw_graphviz.c)

---

## Other features

```cpp
#include <mthpc/spinlock.h>
#include <mthpc/util.h>
#include <mthpc/list.h>
#include <mthpc/debug.h>
#include <mthpc/print.h>
#include <mthpc/guards.h>
```

---

## Future works

- hash table
- mlrcu
