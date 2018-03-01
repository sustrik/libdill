# NAME

brecv - receives data from a bytestream socket

# SYNOPSIS

```c
#include <libdill.h>

int brecv(int s, void* buf, size_t len, int64_t deadline);
```

# DESCRIPTION

This function receives data from a bytestream socket. It is ablocking operation that unblocks only after the requested amount ofdata is received.  There is no such thing as partial receive.If a problem, including timeout, occurs while receiving the data,an error is returned to the user and the socket cannot be used forreceiving from that point on.

If **buf** is **NULL**, **len** bytes will be received but they willbe dropped rather than returned to the user.

**s**: The socket.

**buf**: Buffer to receive the data to.

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
char msg[100];
int rc = brecv(s, msg, sizeof(msg), -1);
```

# SEE ALSO

**brecvl**(3) **bsend**(3) **bsendl**(3) **now**(3) 

