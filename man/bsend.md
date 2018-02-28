# NAME

bsend - sends data to a socket

# SYNOPSIS

```c
#include <libdill.h>

int bsend(int s, const void* buf, size_t len, int64_t deadline);
```

# DESCRIPTION

This function sends data to a bytestream socket. It is a blocking
operation that unblocks only after all the data are sent. There is
no such thing as partial send. If a problem, including timeout,
occurs while sending the data error is returned to the user and the
socket cannot be used for sending from that point on.

**s**: The socket to send the data to.

**buf**: Buffer to send.

**len**: Size of the buffer, in bytes.

**deadline**: A point in time when the operation should time out, in milliseconds. Use the **now** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

# RETURN VALUE

In case of success the function returns 0. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EBADF**: Invalid handle.
* **ECANCELED**: Current coroutine was canceled.
* **ECONNRESET**: Broken connection.
* **EINVAL**: Invalid argument.
* **ENOTSUP**: The handle does not support this operation.
* **EPIPE**: Closed connection.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
int rc = bsend(s, "ABC", 3, -1);
```
# SEE ALSO

**now**(3) 
