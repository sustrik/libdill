# NAME

http_attach_mem - creates HTTP protocol on top of an underlying socket

# SYNOPSIS

**#include &lt;libdill.h>**

**int http_attach_mem(int **_s_**, void **\*_mem_**);**

# DESCRIPTION

**WARNING: This is experimental functionality and the API may change in the future.**

HTTP is an application-level protocol described in RFC 7230. This implementation handles only the request/response exchange. Whatever comes after that must be handled by a different protocol.

This function instantiates HTTP protocol on top of underlying bytestream protocol _s_ in a user-supplied memory. Unless you are hyper-optimizing use **http_attach()** instead.

The memory passed in _mem_ argument must be at least _HTTP\_SIZE_ bytes long and can be deallocated only after the socket is closed.

The socket can be cleanly shut down using **http_detach()** function.

# RETURN VALUE

Newly created socket handle. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EBADF**: Invalid socket handle.
* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOMEM**: Not enough memory.
* **EPROTO**: Underlying socket is not a bytestream socket.

# EXAMPLE

```c
int s = tcp_connect(&addr, -1);
char mem[HTTP_SIZE];
int s = http_attach_mem(s, mem);
```
