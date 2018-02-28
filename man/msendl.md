# NAME

msendl - sends a message

# SYNOPSIS

```c
#include <libdill.h>

int msendl(int s, struct iolist* first, struct iolist* last, int64_t deadline);
```

# DESCRIPTION

This function sends a message to a socket. It is a blocking
operation that unblocks only after entire message is sent.
There is no such thing as partial send. If a problem, including
timeout, occurs while sending the message error is returned to the
user and the socket cannot be used for sending from that point on.

This function accepts a linked list of I/O buffers instead of a
single buffer. Argument **first** points to the first item in the
list, **last** points to the last buffer in the list. The list
represents a single, fragmented message, not a list of multiple
messages. Structure **iolist** has the following members:

    void *iol_base;          /* Pointer to the buffer. */
    size_t iol_len;          /* Size of the buffer. */
    struct iolist *iol_next; /* Next buffer in the list. */
    int iol_rsvd;            /* Reserved. Must be set to zero. */

The function returns **EINVAL** error in case the list is malformed
or if it contains loops.

**s**: The socket to send the message to.

**first**: Pointer to the first item of a linked list of I/O buffers.

**last**: Pointer to the last item of a linked list of I/O buffers.

**deadline**: A point in time when the operation should time out, in milliseconds. Use the **now** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

# RETURN VALUE

In case of success the function returns 0. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EBADF**: Invalid handle.
* **ECANCELED**: Current coroutine was canceled.
* **EINVAL**: Invalid argument.
* **EMSGSIZE**: The message is too long.
* **ENOTSUP**: The handle does not support this operation.
* **EPIPE**: Closed connection.
* **ETIMEDOUT**: Deadline was reached.

# SEE ALSO

**mrecv**(3) **mrecvl**(3) **msend**(3) **now**(3) 
