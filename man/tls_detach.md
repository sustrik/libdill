# NAME

tls_detach - terminates TLS protocol and returns the underlying socket

# SYNOPSIS

```c
#include <libdill.h>

int tls_detach(int s, int64_t deadline);
```

# DESCRIPTION

**WARNING: This is experimental functionality and the API may change in the future.**

TLS is a cryptographic protocol to provide secure communication over
the network. It is a bytestream protocol.

This function does the terminal handshake and returns underlying
socket to the user. The socket is closed even in the case of error.

**s**: Handle of the TLS socket.

**deadline**: A point in time when the operation should time out, in milliseconds. Use the **now** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

This function is not available if libdill is compiled with **--disable-sockets** option.

This function is not available if libdill is compiled without **--enable-tls** option.

# RETURN VALUE

In case of success the function returns underlying socket handle. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EBADF**: Invalid handle.
* **ECANCELED**: Current coroutine was canceled.
* **ECONNRESET**: Broken connection.
* **ENOTSUP**: The handle is not a TLS protocol handle.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
int s = tcp_connect(&addr, -1);
s = tls_attach_client(s, -1);
bsend(s, "ABC", 3, -1);
char buf[3];
ssize_t sz = brecv(s, buf, sizeof(buf), -1);
s = tls_detach(s, -1);
tcp_close(s);
```
# SEE ALSO

brecv(3) brecvl(3) bsend(3) bsendl(3) now(3) tls_attach_client(3) tls_attach_client_mem(3) tls_attach_server(3) tls_attach_server_mem(3) 
