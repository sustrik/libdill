# NAME

yield - yields CPU to other coroutines

# SYNOPSIS

```c
#include <libdill.h>

int yield(void);
```

# DESCRIPTION

By calling this function, you give other coroutines a chance to run.

You should consider using **yield** when doing lengthy computations
which don't have natural coroutine switching points such as socket
or channel operations or msleep.

# RETURN VALUE

In case of success the function returns 0. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **ECANCELED**: Current coroutine was canceled.

# EXAMPLE

```c
for(i = 0; i != 1000000; ++i) {
    expensive_computation();
    yield(); /* Give other coroutines a chance to run. */
}
```
# SEE ALSO

**bundle**(3) **bundle_go**(3) **bundle_go_mem**(3) **bundle_mem**(3) **go**(3) **go_mem**(3) 
