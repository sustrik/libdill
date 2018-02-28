# NAME

http_recvrequest - receives HTTP request from the peer

# SYNOPSIS

```c
#include <libdill.h>

int http_recvrequest(int s, char* command, size_t commandlen, char* resource, size_t resourcelen, int64_t deadline);
```

# DESCRIPTION

**WARNING: This is experimental functionality and the API may change in the future.**

HTTP is an application-level protocol described in RFC 7230. This
implementation handles only the request/response exchange. Whatever
comes after that must be handled by a different protocol.

This function receives an HTTP request from the peer.

**s**: HTTP socket handle.

**command**: Buffer to store HTTP command in.

**commandlen**: Size of the **command** buffer.

**resource**: Buffer to store HTTP resource in.

**resourcelen**: Size of the **resource** buffer.

**deadline**: A point in time when the operation should time out, in milliseconds. Use the **now** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

This function is not available if libdill is compiled with **--disable-sockets** option.

# RETURN VALUE

In case of success the function returns 0. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EBADF**: Invalid handle.
* **ECANCELED**: Current coroutine was canceled.
* **ECONNRESET**: Broken connection.
* **EINVAL**: Invalid argument.
* **EMSGSIZE**: The data won't fit into the supplied buffer.
* **ENOTSUP**: The handle does not support this operation.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
int s = tcp_accept(listener, NULL, -1);
s = http_attach(s, -1);
char command[256];
char resource[256];
http_recvrequest(s, command, sizeof(command), resource, sizeof(resource), -1);
while(1) {
    char name[256];
    char value[256];
    int rc = http_recvfield(s, name, sizeof(name), value, sizeof(value), -1);
    if(rc == -1 && errno == EPIPE) break;
}
http_sendstatus(s, 200, "OK", -1);
s = http_detach(s, -1);
tcp_close(s);
```
# SEE ALSO

http_attach(3) http_attach_mem(3) http_detach(3) http_recvfield(3) http_recvrequest(3) http_recvstatus(3) http_sendfield(3) http_sendrequest(3) http_sendstatus(3) now(3) 
