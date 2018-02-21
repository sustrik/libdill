# NAME

yield - yields CPU to other coroutines

# SYNOPSIS

```c
#include <libdill.h>

int yield(void);
```

# DESCRIPTION

By calling this function, you give other coroutines a chance to run.

You should consider using **yield** when doing lengthy computations which don't have natural coroutine switching points such as socket or channel operations or msleep.


# RETURN VALUE

In case of success the function returns 0. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **ECANCELED**: Current coroutine is in the process of shutting down.

# EXAMPLE

```c
for(i = 0; i != 1000000; ++i) {
    expensive_computation();
    yield(); /* Give other coroutines a chance to run. */
}
```
