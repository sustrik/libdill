# NAME

ipc_connect_mem - creates a connection to remote IPC endpoint

# SYNOPSIS

```c
#include <libdill.h>

int ipc_connect_mem(const char* addr, void* mem, int64_t deadline);
```

# DESCRIPTION

IPC  protocol is a bytestream protocol for transporting data among
processes on the same machine.  It is an equivalent to POSIX
**AF_LOCAL** sockets.

This function creates a connection to a remote IPC endpoint.

This function allows to avoid one dynamic memory allocation by
storing the object in user-supplied memory. Unless you are
hyper-optimizing use **ipc_connect** instead.

**addr**: Filename to connect to.

**mem**: The memory to store the newly created object. It must be at least **IPC_SIZE** bytes long and must not be deallocated before the object is closed.

**deadline**: A point in time when the operation should time out, in milliseconds. Use the **now** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

The socket can be cleanly shut down using **ipc_close** function.

This function is not available if libdill is compiled with **--disable-sockets** option.

# RETURN VALUE

In case of success the function returns newly created socket handle. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EACCES**: The process does not have appropriate privileges.
* **ECANCELED**: Current coroutine was canceled.
* **ECONNREFUSED**: The target address was not listening for connections or refused the connection request.
* **ECONNRESET**: Remote host reset the connection request.
* **EINVAL**: Invalid argument.
* **ELOOP**: A loop exists in symbolic links encountered during resolution of the pathname in address.
* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENAMETOOLONG**: A component of a pathname exceeded **NAME_MAX** characters, or an entire pathname exceeded **PATH_MAX** characters.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOENT**: A component of the pathname does not name an existing file or the pathname is an empty string.
* **ENOMEM**: Not enough memory.
* **ENOTDIR**: A component of the path prefix of the pathname in address is not a directory.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
int s = tcp_connect("/tmp/test.ipc", -1);
bsend(s, "ABC", 3, -1);
char buf[3];
brecv(s, buf, sizeof(buf), -1);
ipc_close(s);
```
# SEE ALSO

**brecv**(3) **brecvl**(3) **bsend**(3) **bsendl**(3) **hclose**(3) **ipc_accept**(3) **ipc_accept_mem**(3) **ipc_close**(3) **ipc_connect**(3) **ipc_listen**(3) **ipc_listen_mem**(3) **now**(3) 
