# NAME

msleep - waits until deadline expires

# SYNOPSIS

```c
#include <libdill.h>

int msleep(int64_t deadline);
```

# DESCRIPTION

This function blocks until the deadline expires or an error occurs.

**deadline**: A point in time when the operation should time out, in milliseconds. Use the **now** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

# RETURN VALUE

In case of success the function returns 0. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **ECANCELED**: Current coroutine was canceled.

# EXAMPLE

```c
            int rc = msleep(now() + 1000);
            if(rc != 0) {
                perror("Cannot sleep");
                exit(1);
            }
            printf("Slept succefully for 1 second.
");
```
# SEE ALSO

now(3) now(3) 
