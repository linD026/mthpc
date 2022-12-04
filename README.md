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

// TODO
```

### workqueue

```cpp
#include <mthpc/workqueue.h>

// TODO
```

### centralized barrier

```cpp
#include <mthpc/centralized_barrier.h>

// TODO
```

###  Read-Copy Update (RCU)

```cpp
#include <mthpc/rcu.h>
#include <mthpc/rculist.h>

// TODO
```

---

## Future works

### Basic

- scoped lock

### intermediate


### Advanced

- hash table
- mlrcu
