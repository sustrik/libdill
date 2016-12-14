# NAME

msleep - waits until deadline expires

# SYNOPSIS

```c
#include <libdill.h>
int msleep(int64_t deadline);
```

# DESCRIPTION

`deadline` is a point in time when the operation should finish. Use `now` function to get current point in time. 0 will cause the function to return without blocking. -1 will cause it to block forever.

# RETURN VALUE

Returns 0 in case of success, -1 in case of error. In the latter case it sets `errno` to one of the values below.

# ERRORS

* `ECANCELED`: Current coroutine is being shut down.

# EXAMPLE

```c
int result = msleep(now() + 1000);
if(result != 0) {
    perror("Cannot sleep");
    exit(1);
}
printf("Slept succefully for 1 second.\n");
```

