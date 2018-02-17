# NAME

http_recvrequest - receives HTTP request from the peer

# SYNOPSIS

**#include &lt;libdill.h>**

**int http_recvrequest(int **_s_**, char **\*_command_**, size_t **_commandlen_**, char **\*_resource_**, size_t **_resourcelen_**, int64_t **_deadline_**);**

# DESCRIPTION

**WARNING: This is experimental functionality and the API may change in the future.**

HTTP is an application-level protocol described in RFC 7230. This implementation handles only the request/response exchange. Whatever comes after that must be handled by a different protocol.

This function receives an HTTP request from the peer. 

_deadline_ is a point in time when the operation should time out. Use the **now()** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

# RETURN VALUE

Zero in the case of success. On error returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EBADF**: Invalid socket handle.
* **ECANCELED**: Current coroutine is in the process of shutting down.
* **ECONNRESET**: Broken connection.
* **EINVAL**: Invalid argument.
* **ENOTSUP**: The handle is not an HTTP protocol handle.
* **EMSGSIZE**: The data won't fit into the supplied buffer.
* **EPROTO**: Network protocol violated by the peer.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
char command[256];
char resource[256];
int rc = http_recvrequest(s, command, sizeof(command), resource, sizeof(resource), -1);
```
