# NAME

http_sendstatus - sends HTTP status line to the peer

# SYNOPSIS

**#include &lt;libdill.h>**

**int http_sendstatus(int **_s_**, int **_status_**, const char **\*_reason_**, int64_t **_deadline_**);**

# DESCRIPTION

**WARNING: This is experimental functionality and the API may change in the future.**

HTTP is an application-level protocol described in RFC 7230. This implementation handles only the request/response exchange. Whatever comes after that must be handled by a different protocol.

This function sends an HTTP status line to the peer. This is meant to be done at the beginning of the HTTP response. For example, if _status_ is 404 and _reason_ is `Not found` the line sent will look like this:

```
HTTP/1.1 404 Not found
```

_deadline_ is a point in time when the operation should time out. Use the **now()** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

# RETURN VALUE

Zero in the case of success. On error returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EBADF**: Invalid socket handle.
* **ECANCELED**: Current coroutine is in the process of shutting down.
* **ECONNRESET**: Broken connection.
* **EINVAL**: Invalid argument.
* **ENOTSUP**: The handle is not an HTTP protocol handle.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
int rc = http_sendstatus(s, 404, "Not found", -1);
```
