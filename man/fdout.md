# NAME

fdout - waits while file descriptor becomes writeable

# SYNOPSIS

```c
#include <libdill.h>
int fdout(int fd, int64_t deadline);
```

# DESCRIPTION

Waits until file descriptor becomes writeable or until it gets into an error state. Both options cause successful return from the function. To distinguish between the two outcomes you have to do subsequent write operation on the file descriptor.

# RETURN VALUE

The function returns 0 in case of success or -1 in case of error. In the latter case is sets `errno` to one of the following values.

# ERRORS

* `EBADF`: Not a file descriptor.
* `ECANCELED`: Current coroutine is being shut down.
* `EEXIST`: Another coroutine is already blocked on `fdout` with this file descriptor.
* `ETIMEDOUT`: Deadline expired while waiting for the file descriptor.

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

