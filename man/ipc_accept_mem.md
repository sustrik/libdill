# NAME

ipc_accept_mem - accepts an incoming IPC connection

# SYNOPSIS


**#include &lt;libdill.h>**

**int ipc_accept_mem(int **_s_**, void **\*_mem_**, int64_t** _deadline_**);**

# DESCRIPTION

IPC protocol is a bytestream protocol (i.e. data can be sent via **bsend()** and received via **brecv()**) for transporting data among processes on the same machine. It is an equivalent to POSIX **AF_LOCAL** sockets.

This function accepts an incoming IPC connection from the listening socket _s_, in user-supplied memory. The memory is passed in _mem_ argument. It must be at least _IPC\_SIZE_ bytes long and can be deallocated only after the socket is closed. Unless you are hyper-optimizing use **ipc_accept()** instead.

_deadline_ is a point in time when the operation should time out. Use the **now()** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

The socket can be cleanly shut down using **ipc_close()** function.

# RETURN VALUE

Newly created socket handle. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EBADF**: Invalid socket handle.
* **ECANCELED**: Current coroutine is being shut down.
* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOMEM**: Not enough memory.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
int listener = ipc_listen("/tmp/test.ipc", 10);
char mem[IPC_SIZE];
int s = ipc_accept_mem(listener, mem, -1);
```
