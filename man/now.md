# NAME

now - get current time

# SYNOPSIS

```c
#include <libdill.h>

int64_t now(void);
```

# DESCRIPTION

Returns current time, in milliseconds.

The function is meant for creating deadlines. For example, a point
of time one second from now can be expressed as **now() + 1000**.

The following values have special meaning and cannot be returned by
the function:

* 0: Immediate deadline.
* -1: Infinite deadline.

# RETURN VALUE

Current time.

# ERRORS

None.

# EXAMPLE

```c
int result = chrecv(ch, &val, sizeof(val), now() + 1000);
if(result == -1 && errno == ETIMEDOUT) {
    printf("One second elapsed without receiving a message.\n");
}
```
# SEE ALSO

msleep(3) 
