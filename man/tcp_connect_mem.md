# NAME

tcp_connect_mem - creates a connection to remote TCP endpoint 

# SYNOPSIS

```c
#include <libdill.h>

int tcp_connect_mem(const struct ipaddr* addr, void* mem, int64_t deadline);
```

# DESCRIPTION

TCP protocol is a reliable bytestream protocol for transporting data
over network. It is defined in RFC 793.

This function creates a connection to a remote TCP endpoint.

This function allows to avoid one dynamic memory allocation by
storing the object in user-supplied memory. Unless you are
hyper-optimizing use **tcp_connect** instead.

**addr**: IP address to connect to.

**mem**: The memory to store the newly created object. It must be at least **TCP_SIZE** bytes long and must not be deallocated before the object is closed.

**deadline**: A point in time when the operation should time out, in milliseconds. Use the **now** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

The socket can be cleanly shut down using **tcp_close** function.

This function is not available if libdill is compiled with **--disable-sockets** option.

# RETURN VALUE

In case of success the function returns newly created socket handle. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **ECANCELED**: Current coroutine is in the process of shutting down.
* **ECONNREFUSED**: The target address was not listening for connections or refused the connection request.
* **ECONNRESET**: Remote host reset the connection request.
* **EHOSTUNREACH**: The destination host cannot be reached.
* **EINVAL**: Invalid argument.
* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENETDOWN**: The local network interface used to reach the destination is down.
* **ENETUNREACH**: No route to the network is present.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOMEM**: Not enough memory.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
struct ipaddr addr;
ipaddr_remote(&addr, "www.example.org", 80, 0, -1);
int s = tcp_connect(&addr, -1);
bsend(s, "ABC", 3, -1);
char buf[3];
brecv(s, buf, sizeof(buf), -1);
tcp_close(s);
```
