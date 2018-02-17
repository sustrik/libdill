# NAME

tls_attach_client - creates TLS protocol on top of an underlying socket

# SYNOPSIS

**#include &lt;libdill.h>**

**int tls_attach_client(int **_s_**, int64_t **_deadline_**);**

# DESCRIPTION

**WARNING: This is experimental functionality and the API may change in the future.**

TLS is a cryptographic protocol to provide secure communication over the network. It is a bytestream protocol.

This function instantiates TLS protocol on top of underlying bytestream protocol _s_. TLS protocol being asymmetric, client and server sides are intialized in different ways. This particular function initializes the client side.

The socket can be cleanly shut down using **tls_detach()** function.

This function is available only if libdill library is built with `--enable-tls` option.

# RETURN VALUE

Newly created socket handle. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EBADF**: Invalid socket handle.
* **ECANCELED**: Current coroutine is in the process of shutting down.
* **ECONNRESET**: Broken connection.
* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOMEM**: Not enough memory.
* **EPROTO**: Underlying socket is not a bytestream socket.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
int s = tcp_connect(&addr, -1);
s = tls_attach_client(s, -1);
```
