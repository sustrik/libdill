# NAME

ipc_pair - creates a pair of mutually connected IPC sockets

# SYNOPSIS

**#include &lt;libdill.h>**

**int ipc_pair(int **_s_**[2]);**

# DESCRIPTION

IPC protocol is a bytestream protocol (i.e. data can be sent via **bsend()** and received via **brecv()**) for transporting data among processes on the same machine. It is an equivalent to POSIX **AF_LOCAL** sockets.

This function creates a pair of mutually connected IPC sockets.

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
int rc = ipc_pair(s);
```
