# NAME

ipc_close - closes IPC connection in an orderly manner

# SYNOPSIS

```c
#include <libdill.h>

int ipc_close(int s, int64_t deadline);
```

# DESCRIPTION

IPC  protocol is a bytestream protocol for transporting data among
processes on the same machine.  It is an equivalent to POSIX
**AF_LOCAL** sockets.

This function closes a IPC socket cleanly. Unlike **hclose** it lets
the peer know that it is shutting down and waits till the peer
acknowledged the shutdown. If this terminal handshake cannot be
done it returns error. The socket is closed even in the case of
error.

It can also be used to close IPC listener socket in which case it's
equivalent to calling **hclose**.

**s**: The IPC socket.

**deadline**: A point in time when the operation should time out, in milliseconds. Use the **now** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

This function is not available if libdill is compiled with **--disable-sockets** option.

# RETURN VALUE

In case of success the function returns 0. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EBADF**: Invalid handle.
* **ECANCELED**: Current coroutine was canceled.
* **ECONNRESET**: Broken connection.
* **ENOTSUP**: The handle does not support this operation.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
int ls = ipc_listen("/tmp/test.ipc", 10);
int s = ipc_accept(ls, -1);
bsend(s, "ABC", 3, -1);
char buf[3];
brecv(s, buf, sizeof(buf), -1);
ipc_close(s);
ipc_close(ls);
```
# SEE ALSO

brecv(3) brecvl(3) bsend(3) bsendl(3) ipc_accept(3) ipc_accept_mem(3) ipc_connect(3) ipc_connect_mem(3) ipc_listen(3) ipc_listen_mem(3) now(3) 
