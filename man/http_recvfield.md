# NAME

http_recvfield - receives HTTP field from the peer

# SYNOPSIS

```c
#include <libdill.h>

int http_recvfield(int s, char* name, size_t namelen, char* value, size_t valuelen, int64_t deadline);
```

# DESCRIPTION

**WARNING: This is experimental functionality and the API may change in the future.**

HTTP is an application-level protocol described in RFC 7230. This
implementation handles only the request/response exchange. Whatever
comes after that must be handled by a different protocol.

This function receives an HTTP field from the peer.

**s**: HTTP socket handle.

**name**: Buffer to store name of the field.

**namelen**: Size of the **name** buffer.

**value**: Buffer to store value of the field.

**valuelen**: Size of the **value** buffer.

**deadline**: A point in time when the operation should time out, in milliseconds. Use the **now** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

This function is not available if libdill is compiled with **--disable-sockets** option.

# RETURN VALUE

In case of success the function returns 0. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EBADF**: Invalid socket handle.
* **ECANCELED**: Current coroutine is in the process of shutting down.
* **ECONNRESET**: Broken connection.
* **EINVAL**: Invalid argument.
* **EMSGSIZE**: The data won't fit into the supplied buffer.
* **ENOTSUP**: The handle does not support this operation.
* **EPIPE**: There are no more fields to receive.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
int s = tcp_connect(&addr, -1);
s = http_attach(s);
http_sendrequest(s, "GET", "/", -1);
http_sendfield(s, "Host", "www.example.org", -1);
hdone(s, -1);
char reason[256];
http_recvstatus(s, reason, sizeof(reason), -1);
while(1) {
    char name[256];
    char value[256];
    int rc = http_recvfield(s, name, sizeof(name), value, sizeof(value), -1);
    if(rc == -1 && errno == EPIPE) break;
}
s = http_detach(s, -1);
tcp_close(s);
```
