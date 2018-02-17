# NAME

http_sendfield - sends HTTP field to the peer

# SYNOPSIS

**#include &lt;libdill.h>**

**int http_sendfield(int **_s_**, const char **\*_name_**, const char **\*_value_**, int64_t **_deadline_**);**

# DESCRIPTION

**WARNING: This is experimental functionality and the API may change in the future.**

HTTP is an application-level protocol described in RFC 7230. This implementation handles only the request/response exchange. Whatever comes after that must be handled by a different protocol.

This function sends an HTTP field, i.e. a name/value pair, to the peer. For example, if _name_ is `Host` and _resource_ is `www.example.org` the line sent will look like this:

```
Host: www.example.org
```

After sending the last field of HTTP request don't forget to call _hdone()_ on the socket. It will send an empty line to the server to let it know that the request is finished and it should start processing it. 

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
int rc = http_sendfield(s, "Host", "www.example.org", -1);
```
