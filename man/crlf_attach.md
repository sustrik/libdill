# NAME

crlf_attach - creates CRLF protocol on top of underlying socket

# SYNOPSIS

**#include &lt;libdill.h>**

**int crlf_attach(int **_s_**);**

# DESCRIPTION

CRLF is a message-based protocol that delimits messages usign CR+LF byte sequence (0x0D 0x0A). In other words, it's a protocol to send text messages separated by newlines. The protocol has no initial handshake. Terminal handshake is accomplished by each peer sending an empty line.

This function instantiates CRLF protocol on top of underlying bytestream protocol _s_.

The socket can be cleanly shut down using **crlf_detach()** function.

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
int u = tcp_connect(&addr, -1);
int s = crlf_attach(u);
```
