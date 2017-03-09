# NAME

tcp_close - closes TCP connection in an orderly manner

# SYNOPSIS

**#include &lt;libdill.h>**

**int tcp_close(int **_s_**, int64_t** _deadline_**);**

# DESCRIPTION

TCP protocol is a bytestream protocol (i.e. data can be sent via **bsend()** and received via **brecv()**) for transporting data among machines.

This function closes a TCP socket cleanly. Unlike **hclose()** it lets the peer know that it is shutting down and waits till the peer acknowledged the shutdown. If this terminal handshake cannot be done it returns error.

_deadline_ is a point in time when the operation should time out. Use the **now()** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

# RETURN VALUE

Zero in case of success. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EBADF**: Invalid handle.
* **ECANCELED**: Current coroutine is being shut down.
* **ECONNRESET**: Broken connection.
* **ENOTSUP**: The operation is not supported.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
int rc = tcp_close(s, -1);
```
