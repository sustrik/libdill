# NAME

brecv, brecvl - receive data from bytestream socket

# SYNOPSIS

**#include &lt;libdill.h>**

**int brecv(int **_s_**, void **\*_buf_**, size_t **_len_**, int64_t **_deadline_**);**

**int brecvl(int **_s_**, struct iolist **\*_first_**, struct iolist **\*_last_**, int64_t **_deadline_**);**

# DESCRIPTION

Function **brecv()** receives data from a bytestream socket. It is a blocking operation that unblocks only after the requested amount of data is received. There is no such thing as partial receive. If a problem, including timeout, occurs while receiving the data, an error is returned to the user and the socket cannot be used for receiving from that point on.

_buf_ points to the buffer _len_ bytes long to write the received data into. If _buf_ is **NULL**, _len_ bytes will be received but they will be dropped rather than returned to the user.

_deadline_ is a point in time when the operation should time out. Use the **now()** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

Function **brecvl()** accepts a linked list of buffers instead of a single buffer. _first_ points to the first item in the list, _last_ points to the last item in the list. Structure **iolist** has the following members:

```
void *iol_base;          /* Pointer to the buffer. */
size_t iol_len;          /* Size of the buffer. */
struct iolist *iol_next; /* Next buffer. */
int iol_rsvd;            /* Reserved. Must be set to zero. */
```

**brecvl()** returns **EINVAL** in case the list is malformed:

* If _first_ is **NULL**.
* If _last_ is **NULL**.
* If _last_->_iol\_next_ is not **NULL**.
* If _first_ and _last_ don't belong to the same list.
* If there's a loop in the list.
* If _iol\_rsvd_ of any item is non-zero.

The list can be temporarily modified while the function is in progress. However, once the function returns the list is guaranteed to be the same as before the call.

# RETURN VALUE

The function returns 0 on success. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EBADF**: The socket handle in invalid.
* **ECANCELED**: Current coroutine is in the process of shutting down.
* **ECONNRESET**: Broken connection.
* **EINVAL**: Invalid arguments.
* **ENOMEM**: Not enough memory.
* **ENOTSUP**: The operation is not supported by the socket.
* **EPIPE**: Closed connection.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
char buf[100];
int rc = brecv(s, buf, sizeof(buf), -1);
```
