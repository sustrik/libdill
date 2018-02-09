# NAME

ipc_pair_mem - creates a pair of mutually connected IPC sockets

# SYNOPSIS

**#include &lt;libdill.h>**

**int ipc_pair_mem(void **\*_mem_**, int **_s_**[2]);**

# DESCRIPTION

IPC protocol is a bytestream protocol (i.e. data can be sent via **bsend()** and received via **brecv()**) for transporting data among processes on the same machine. It is an equivalent to POSIX **AF_LOCAL** sockets.

This function creates a pair of mutually connected IPC sockets, in user-supplied memory. The memory is passed in _mem_ argument. It must be at least _IPC\_PAIR\_SIZE_ bytes long and can be deallocated only after both sockets are closed. Unless you are hyper-optimizing use **ipc_pair()** instead.

The sockets can be cleanly shut down using **ipc_close()** function.

# RETURN VALUE

Zero in case of success. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EACCES**: The process does not have appropriate privileges.
* **ECANCELED**: Current coroutine is being shut down.
* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOMEM**: Not enough memory.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
int s[2];
char mem[IPC_PAIR_SIZE];
int rc = ipc_pair_mem(mem, s);
```
