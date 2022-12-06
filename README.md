# mthpc

mthpc is parallelism and concurrency library for C language. It targets to
improve the parallel and concurrent part of userspace C. mtphc provides the
thread framework, asynchronous worker (deferred work), synchronous mechanisms,
and some concurrent data structures. mtphc is based on [GCC builtin atomic operation](https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html)
and [POSIX Threads](https://en.wikipedia.org/wiki/Pthreads).

## Features

mthpc supports the following features.

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

```cpp
void mthpc_thread_run(&group_object /* or thread object */);
```

#### Examples

* [thread self-test](src/thread/test.c)
* [rcu self-test](src/rcu/test.c)


### workqueue

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

* [workqueue self-test](src/workqueue/test.c)


### centralized barrier

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

* [rcu self-test](src/rcu/test.c)

---

## Future works

### Basic

- scoped lock

### intermediate


### Advanced

- hash table
- mlrcu
