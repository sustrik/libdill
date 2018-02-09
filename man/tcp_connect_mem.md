# NAME

tcp_connect_mem - create a connection to remote TCP endpoint

# SYNOPSIS


**#include &lt;libdill.h>**

**int tcp_connect_mem(const struct ipaddr **\*_addr_**, void **\*_mem_**, int64_t** _deadline_**);**

# DESCRIPTION

TCP protocol is a bytestream protocol (i.e. data can be sent via **bsend()** and received via **brecv()**) for transporting data among machines.

Creates a connection to a peer specified by _addr_ argument, in user-supplied memory. The memory is passed in _mem_ argument. It must be at least _TCP\_SIZE_ bytes long and can be deallocated only after the socket is closed. Unless you are hyper-optimizing use **tcp_connect()** instead.

_deadline_ is a point in time when the operation should time out. Use the **now()** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

The socket can be cleanly shut down using **tcp_close()** function.

# RETURN VALUE

Newly created socket handle. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **ECANCELED**: Current coroutine is being shut down.
* **ECONNREFUSED**:The target address was not listening for connections or refused the connection request.
* **ECONNRESET**: Remote host reset the connection request.
* **EHOSTUNREACH**: The destination host cannot be reached.
* **EINVAL**: Invalid arguments.
* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENETDOWN**: The local network interface used to reach the destination is down.
* **ENETUNREACH**: No route to the network is present.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOMEM**: Not enough memory.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
char mem[TCP_SIZE];
int s = tcp_connect_mem(&addr, mem, -1);
```
