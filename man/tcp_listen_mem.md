# NAME

tcp_listen_mem - starts listening for incoming TCP connections

# SYNOPSIS

```c
#include <libdill.h>

int tcp_listen_mem(const struct ipaddr* addr, int backlog, void* mem);
```

# DESCRIPTION

TCP protocol is a reliable bytestream protocol for transporting data
over network. It is defined in RFC 793.

This function starts listening for incoming connections.
The connections can be accepted using **tcp_accept** function.

This function allows to avoid one dynamic memory allocation by
storing the object in user-supplied memory. Unless you are
hyper-optimizing use **tcp_listen** instead.

**addr**: IP address to listen on.

**backlog**: Maximum number of connections that can be kept open without accepting them.

**mem**: The memory to store the newly created object. It must be at least **TCP_LISTENER_SIZE** bytes long and must not be deallocated before the object is closed.

The socket can be closed either by **hclose** or **tcp_close**.
Both ways are equivalent.

This function is not available if libdill is compiled with **--disable-sockets** option.

# RETURN VALUE

In case of success the function returns newly created socket. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EADDRINUSE**: The specified address is already in use.
* **EADDRNOTAVAIL**: The specified address is not available from the local machine.
* **EINVAL**: Invalid argument.
* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOMEM**: Not enough memory.

# EXAMPLE

```c
struct ipaddr addr;
ipaddr_local(&addr, NULL, 5555, 0);
int ls = tcp_listen(&addr, 10);
int s = tcp_accept(ls, NULL, -1);
bsend(s, "ABC", 3, -1);
char buf[3];
brecv(s, buf, sizeof(buf), -1);
tcp_close(s);
tcp_close(ls);
```
# SEE ALSO

tcp_accept(3) tcp_accept_mem(3) tcp_close(3) tcp_connect(3) tcp_connect_mem(3) tcp_listen(3) tcp_listen_mem(3) 
