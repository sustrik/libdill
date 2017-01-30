# NAME

msleep - waits until deadline expires

# SYNOPSIS

**#include &lt;libdill.h>**

**int msleep(int64_t** _deadline_**);**

# DESCRIPTION

_deadline_ is a point in time when the operation should finish. Use the **now** function to get your current point in time. 0 will cause the function to return without blocking. -1 will cause it to block forever.

# RETURN VALUE

Returns 0 on successs. One error, -1 is returned and _errno_ is set to one of the values below.

# ERRORS

* **ECANCELED**: Current coroutine is being shut down.

# EXAMPLE

```c
int result = msleep(now() + 1000);
if(result != 0) {
    perror("Cannot sleep");
    exit(1);
}
printf("Slept succefully for 1 second.\n");
```

