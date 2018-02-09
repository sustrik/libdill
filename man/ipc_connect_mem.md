# NAME

ipc_connect_mem - create a connection to remote IPC endpoint

# SYNOPSIS


**#include &lt;libdill.h>**

**int ipc_connect_mem(const char **\*_addr_**, void **\*_mem_**, int64_t** _deadline_**);**

# DESCRIPTION

IPC protocol is a bytestream protocol (i.e. data can be sent via **bsend()** and received via **brecv()**) for transporting data among processes on the same machine. It is an equivalent to POSIX **AF_LOCAL** sockets.

Creates a connection to a peer listening on the file specified by _addr_ argument, in user-supplied memory. The memory is passed in _mem_ argument. It must be at least _IPC\_SIZE_ bytes long and can be deallocated only after the socket is closed. Unless you are hyper-optimizing use **ipc_connect()** instead.

_deadline_ is a point in time when the operation should time out. Use the **now()** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

The socket can be cleanly shut down using **ipc_close()** function.

# RETURN VALUE

Newly created socket handle. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EACCES**: The process does not have appropriate privileges.
* **ECANCELED**: Current coroutine is being shut down.
* **ECONNREFUSED**:The target address was not listening for connections or refused the connection request.
* **ECONNRESET**: Remote host reset the connection request.
* **EINVAL**: Invalid arguments.
* **ELOOP**: A loop exists in symbolic links encountered during resolution of the pathname in address.
* **ENAMETOOLONG**: A component of a pathname exceeded NAME_MAX characters, or an entire pathname exceeded PATH_MAX characters.
* **ENOENT**: A component of the pathname does not name an existing file or the pathname is an empty string.
* **ENOTDIR**: A component of the path prefix of the pathname in address is not a directory.
* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOMEM**: Not enough memory.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
char mem[IPC_SIZE];
int s = ipc_connect_mem("/tmp/test.ipc", mem, -1);
```
