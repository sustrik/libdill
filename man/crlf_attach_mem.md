# NAME

crlf_attach_mem - creates CRLF protocol on top of underlying socket

# SYNOPSIS

```c
#include <libdill.h>

int crlf_attach_mem(int s, void* mem);
```

# DESCRIPTION

CRLF is a message-based protocol that delimits messages usign CR+LF byte
sequence (0x0D 0x0A). In other words, it's a protocol to send text
messages separated by newlines. The protocol has no initial handshake.
Terminal handshake is accomplished by each peer sending an empty line.

This function instantiates CRLF protocol on top of the underlying
protocol.

This function allows to avoid one dynamic memory allocation by
storing the object in user-supplied memory. Unless you are
hyper-optimizing use **crlf_attach** instead.

**s**: Handle of the underlying socket. It must be a bytestream protocol.

**mem**: The memory to store the newly created object. It must be at least **CRLF_SIZE** bytes long and must not be deallocated before the object is closed.

The socket can be cleanly shut down using **crlf_detach** function.

This function is not available if libdill is compiled with **--disable-sockets** option.

# RETURN VALUE

In case of success the function returns newly created socket handle. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EBADF**: Invalid handle.
* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOMEM**: Not enough memory.
* **ENOTSUP**: The handle does not support this operation.
* **EPROTO**: Underlying socket is not a bytestream socket.

# EXAMPLE

```c
int s = tcp_connect(&addr, -1);
s = crlf_attach(s);
msend(s, "ABC", 3, -1);
char buf[256];
ssize_t sz = mrecv(s, buf, sizeof(buf), -1);
s = crlf_detach(s, -1);
tcp_close(s);
```
# SEE ALSO

crlf_attach(3) crlf_attach_mem(3) crlf_detach(3) 
