# NAME

mrecv - receives a message

# SYNOPSIS

```c
#include <libdill.h>

ssize_t mrecv(int s, void* buf, size_t len, int64_t deadline);
```

# DESCRIPTION

This function receives message from a socket. It is a blockingoperation that unblocks only after entire message is received.There is no such thing as partial receive. Either entire messageis received or no message at all.

**s**: The socket.

**buf**: Buffer to receive the message to.

**len**: Size of the buffer, in bytes.

**deadline**: A point in time when the operation should time out, in milliseconds. Use the **now** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

# RETURN VALUE

In case of success the function returns size of the received message, in bytes. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EBADF**: Invalid handle.
* **ECANCELED**: Current coroutine was canceled.
* **ECONNRESET**: Broken connection.
* **EINVAL**: Invalid argument.
* **EMSGSIZE**: The data won't fit into the supplied buffer.
* **ENOTSUP**: The handle does not support this operation.
* **EPIPE**: Closed connection.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
char msg[100];
ssize_t msgsz = mrecv(s, msg, sizeof(msg), -1);
```

# SEE ALSO

**mrecvl**(3) **msend**(3) **msendl**(3) **now**(3) 

