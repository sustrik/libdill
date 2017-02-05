# NAME

fdin - waits on a filedescriptor to become readable

# SYNOPSIS

**#include &lt;libdill.h>**

**int fdin(int** _fd_, **int64_t** _deadline_**);**

# DESCRIPTION

Waits on a file descriptor to either become readable or get into an error state.  Either case leads to a successful return from the function.  To distinguish the two outcomes, follow up with a read operation on the file descriptor.

_deadline_ is a point in time when the operation should time out. Use the **now** function to get your current point in time. 0 means immediate timeout, i.e.,  perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if need be.

# RETURN VALUE

The function returns 0 on success. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EBADF**: Not a file descriptor.
* **EBUSY**: Another coroutine already blocked on **fdin** with this file descriptor.
* **ECANCELED**: Current coroutine is being shut down.
* **ETIMEDOUT**: Deadline expired while waiting on the file descriptor.

# EXAMPLE

```c
int result = fcntl(fd, F_SETFL, O_NONBLOCK); assert(result == 0);
while(1) {
    result = fdin(fd, -1); assert(result == 0);
    char buf[1024];
    ssize_t len = recv(fd, buf, sizeof(buf), 0);
    assert(len > 0);
    process_input(buf, len);
}
```

