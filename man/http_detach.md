# NAME

http_detach - terminates HTTP protocol and returns the underlying socket

# SYNOPSIS

**#include &lt;libdill.h>**

**int http_detach(int **_s_**, int64_t **_deadline_**);**

# DESCRIPTION

**WARNING: This is experimental functionality and the API may change in the future.**

HTTP is an application-level protocol described in RFC 7230. This implementation handles only the request/response exchange. Whatever comes after that must be handled by a different protocol.

This function does the terminal handshake and returns underlying socket to the user. The socket is closed even in the case of error.

_deadline_ is a point in time when the operation should time out. Use the **now()** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

# RETURN VALUE

Underlying socket handle. On error returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EBADF**: Invalid socket handle.
* **ECANCELED**: Current coroutine is in the process of shutting down.
* **ECONNRESET**: Broken connection.
* **ENOTSUP**: The handle is not an HTTP protocol handle.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
int s = http_detach(s);
```
