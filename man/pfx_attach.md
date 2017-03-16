# NAME

pfx_attach - creates PFX protocol on top of underlying socket

# SYNOPSIS

**#include &lt;libdill.h>**

**int pfx_attach(int **_s_**);**

# DESCRIPTION

TODO

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
int s = pfx_attach(u);
```
