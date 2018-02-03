# NAME

tcp_listen - start listening for incoming TCP connections

# SYNOPSIS


**#include &lt;libdill.h>**

**int tcp_listen(const struct ipaddr **\*_addr_**, int** _backlog_**);**

# DESCRIPTION

TCP protocol is a bytestream protocol (i.e. data can be sent via **bsend()** and received via **brecv()**) for transporting data among machines.

This function starts listening for incoming connection on the address specified in _addr_ argument. _backlog_ is the maximum number of connections that can be held open without user accepting them.

The socket can be cleanly shut down using **tcp_close()** function.

# RETURN VALUE

The function returns the handle of the listening socket. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EADDRINUSE**: The specified address is already in use.
* **EADDRNOTAVAIL**: The specified address is not available from the local machine.
* **ECANCELED**: Current coroutine is being shut down.
* **EINVAL**: Invalid arguments.
* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOMEM**: Not enough memory.

# EXAMPLE

```c
struct ipaddr addr;
int rc = ipaddr_local(&addr, NULL, 5555, 0);
int s = tcp_listen(&addr, -1);
```
