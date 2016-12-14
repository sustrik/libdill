# NAME

fdin - waits until file descriptor becomes readable

# SYNOPSIS

```c
#include <libdill.h>
int fdin(int fd, int64_t deadline);
```

# DESCRIPTION

Waits until file descriptor becomes readable or until it gets into an error state. Both options cause successful return from the function. To distinguish between the two outcomes you have to do subsequent read operation on the file descriptor.

`deadline` is a point in time when the operation should time out. Use `now` function to get current point in time. 0 means immediate timeout, i.e. return immediately if file descriptor is readable, return without blocking if it is not. -1 means no deadline, i.e. the call will block forever, if needed.

# RETURN VALUE

The function returns 0 in case of success or -1 in case of error. In the latter case is sets `errno` to one of the following values.

# ERRORS

* `EBADF`: Not a file descriptor.
* `ECANCELED`: Current coroutine is being shut down.
* `EEXIST`: Another coroutine is already blocked on `fdin` with this file descriptor.
* `ETIMEDOUT`: Deadline expired while waiting for the file descriptor.

# EXAMPLE

```c
int result = fcntl(fd, F_SETFL, O_NONBLOCK);
assert(result == 0);
while(1) {
    result = fdin(fd, -1);
    assert(result == 0);
    char buf[1024];
    ssize_t len = recv(fd, buf, sizeof(buf), 0);
    assert(len > 0);
    process_input(buf, len);
}
```

