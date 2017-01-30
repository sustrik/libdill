# NAME

yield - yields CPU to other coroutines

# SYNOPSIS

**#include &lt;libdill.h>**

**int yield(void);**

# DESCRIPTION

By calling this function, you give other coroutines a chance to run.

You should consider using **yield()** when doing lengthy computations which don't have natural coroutine switching points such as **chsend**, **chrecv**, **fdin**, **fdout** or **msleep**.

# RETURN VALUE

The function returns 0 on success and -1 on error. In the latter case, it sets _errno_ to one of the following values.

# ERRORS

* **ECANCELED**: Current coroutine is being shut down.

# EXAMPLE

```c
for(i = 0; i != 1000000; ++i) {
    expensive_computation();
    yield(); /* Give other coroutines a chance to run. */
}
```

