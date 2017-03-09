# NAME

tcp_accept - accepts an incoming TCP connection

# SYNOPSIS


**#include &lt;libdill.h>**

**int tcp_accept(int **_s_**, int64_t** _deadline_**);**

# DESCRIPTION

TCP protocol is a bytestream protocol (i.e. data can be sent via **bsend()** and received via **brecv()**) for transporting data among machines.

This function accepts an incoming TCP connection from the listening socket _s_.

_deadline_ is a point in time when the operation should time out. Use the **now()** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

The socket can be cleanly shut down using **tcp_close()** function.

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
int listener = tcp_listen(&addr, 10);
int s = tcp_accept(listener, -1);
```
