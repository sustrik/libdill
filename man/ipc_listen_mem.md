# NAME

ipc_listen_mem - start listening for incoming IPC connections

# SYNOPSIS


**#include &lt;libdill.h>**

**int ipc_listen_mem(const char **\*_addr_**, int** _backlog_**, void **\*_mem_**);**

# DESCRIPTION

IPC protocol is a bytestream protocol (i.e. data can be sent via **bsend()** and received via **brecv()**) for transporting data among processes on the same machine. It is an equivalent to POSIX **AF_LOCAL** sockets.

This function starts listening for incoming connection on file specified in _addr_ argument. _backlog_ is the maximum number of connections that can be held open without user accepting them.

The socket is allocated in user-supplied memory. The memory is passed in _mem_ argument. It must be at least _IPC\_LISTENER\_SIZE_ bytes long and can be deallocated only after the socket is closed. Unless you are hyper-optimizing use **ipc_listen()** instead.

The socket can be cleanly shut down using **ipc_close()** function.

# RETURN VALUE

The function returns the handle of the listening socket. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EACCES**: The process does not have appropriate privileges.
* **EADDRINUSE**: The specified address is already in use.
* **ECANCELED**: Current coroutine is being shut down.
* **EINVAL**: Invalid arguments.
* **ELOOP**: A loop exists in symbolic links encountered during resolution of the pathname in address.
* **ENAMETOOLONG**: A component of a pathname exceeded NAME_MAX characters, or an entire pathname exceeded PATH_MAX characters.
* **ENOENT**: A component of the pathname does not name an existing file or the pathname is an empty string.
* **ENOTDIR**: A component of the path prefix of the pathname in address is not a directory.
* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOMEM**: Not enough memory.

# EXAMPLE

```c
char mem[IPC_LISTENER_SIZE];
int s = ipc_listen_mem("/tmp/test.ipc", 10, mem);
```
