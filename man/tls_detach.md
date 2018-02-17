# NAME

tls_detach - terminates TLS protocol and returns the underlying socket

# SYNOPSIS

**#include &lt;libdill.h>**

**int tls_detach(int **_s_**, int64_t **_deadline_**);**

# DESCRIPTION

**WARNING: This is experimental functionality and the API may change in the future.**

TLS is a cryptographic protocol to provide secure communication over the network. It is a bytestream protocol.

This function does the terminal handshake with the peer and returns the underlying socket to the user.

In the case of error the socket, as well as the entire underlying network stack, is closed.

This function is available only if libdill library is built with `--enable-tls` option.

# RETURN VALUE

Underlying socket handle. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EBADF**: Invalid socket handle.
* **ECANCELED**: Current coroutine is in the process of shutting down.
* **ECONNRESET**: Broken connection.
* **ENOTSUP**: The handle is not an TLS protocol handle.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
s = tls_detach(s, -1);
```
