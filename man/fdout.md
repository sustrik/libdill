# NAME

fdout - wait on file descriptor to become writable

# SYNOPSIS

**#include &lt;libdill.h>**
**int fdout(int** _fd_**, int64_t** _deadline_**);**

# DESCRIPTION

Waits on a file descriptor (true OS file descriptor, not libdill socket handle) to either become readable or get into an error state.  Either case leads to a successful return from the function.  To distinguish the two outcomes, follow up with a write operation on the file descriptor.

_deadline_ is a point in time when the operation should time out. Use the **now** function to get your current point in time. 0 means immediate timeout, i.e.,  perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if need be.

# RETURN VALUE

The function returns 0 on success. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EBADF**: Not a file descriptor.
* **EBUSY**: Another coroutine already blocked on **fdout** with this file descriptor.
* **ECANCELED**: Current coroutine is being shut down.
* **ETIMEDOUT**: Deadline expired while waiting on the file descriptor. 

# EXAMPLE

```c
void sendbuf(int fd, const char *buf, size_t len) {
    int result = fcntl(fd, F_SETFL, O_NONBLOCK);
    assert(result == 0);
    while(len) {
        result = fdout(fd, -1);
        assert(result == 0);
        ssize_t sent = send(fd, buf, len, 0);
        assert(len > 0);
        buf += sent;
        len -= sent;
    }
}
```

