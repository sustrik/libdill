# NAME

bundle - create an empty coroutine bundle in user-supplied memory

# SYNOPSIS

**#include &lt;libdill.h>**

**int bundle_mem(void **\*_mem_**);**

# DESCRIPTION

Coroutines are always running in bundles. Even coroutine created by **go()** gets its own bundle. Bundle is a lifetime control mechanism. When it is closed all coroutines within the bundle are canceled.

This function creates an empty bundle. Coroutines can be added to the bundle using **bundle_go()** and **bundle_go_mem()** functions.

_mem_ is the memory of at least _BUNDLE\_SIZE_ bytes to store the bundle in. The memory must not be deallocated before all handles referring to the bundle are closed with the **hclose** function, or the behavior is undefined.

Do not use this function unless you are hyper-optimizing your code and you want to avoid a single memory allocation per bundle. Whenever possible, use **bundle** instead.


# RETURN VALUE

Returns a handle to the bundle. In the case of an error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **ECANCELED**: Current coroutine is in the process of shutting down.
* **ENOMEM**: Not enough memory to allocate a new bundle.

# EXAMPLE

```c
char mem[BUNDLE_SIZE];
int b = bundle_mem(mem);
bundle_go(b, worker());
bundle_go(b, worker());
bundle_go(b, worker());
msleep(now() + 1000);
/* Cancel any workers that are still running. */
hclose(b);
```

