# NAME

pfx_attach_mem - creates PFX protocol on top of underlying socket

# SYNOPSIS

**#include &lt;libdill.h>**

**int pfx_attach_mem(int **_s_**, void **\*_mem_**);**

# DESCRIPTION

PFX is a message-based protocol to send binary messages prefixed by 8-byte size in network byte order. The protocol has no initial handshake. Terminal handshake is accomplished by each peer sending eight 0xFF bytes.

This function instantiates PFX protocol on top of underlying bytestream protocol _s_, in a user-supplied memory. Unless you are hyper-optimizing use **pfx_attach()** instead.

The memory passed in _mem_ argument must be at least _PFX\_SIZE_ bytes long and can be deallocated only after the socket is closed.

The socket can be cleanly shut down using **pfx_detach()** function.

# RETURN VALUE

Newly created socket handle. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EBADF**: Invalid socket handle.
* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOMEM**: Not enough memory.
* **EPROTO**: Undrlying socket is not a bytestream socket.

# EXAMPLE

```c
int u = tcp_connect(&addr, -1);
char mem[PFX_SIZE];
int s = pfx_attach_mem(u, mem);
```
