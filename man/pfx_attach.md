# NAME

pfx_attach - creates PFX protocol on top of underlying socket

# SYNOPSIS

```c
#include <libdill.h>

int pfx_attach(int s);
```

# DESCRIPTION

PFX  is a message-based protocol to send binary messages prefixed by
8-byte size in network byte order. The protocol has no initial
handshake. Terminal handshake is accomplished by each peer sending eight
0xFF bytes.

This function instantiates PFX protocol on top of the underlying
protocol.

**s**: Handle of the underlying socket. It must be a bytestream protocol.

The socket can be cleanly shut down using **pfx_detach** function.

This function is not available if libdill is compiled with **--disable-sockets** option.

# RETURN VALUE

In case of success the function returns newly created socket handle. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EBADF**: Invalid socket handle.
* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOMEM**: Not enough memory.
* **ENOTSUP**: The handle does not support this operation.
* **EPROTO**: Underlying socket is not a bytestream socket.

# EXAMPLE

```c
int s = tcp_connect(&addr, -1);
s = pfx_attach(s);
msend(s, "ABC", 3, -1);
char buf[256];
ssize_t sz = mrecv(s, buf, sizeof(buf), -1);
s = pfx_detach(s, -1);
tcp_close(s);
```
