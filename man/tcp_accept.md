# NAME

tcp_accept - accepts an incoming TCP connection

# SYNOPSIS

```c
#include <libdill.h>

int tcp_accept(int s, struct ipaddr* addr, int64_t deadline);
```

# DESCRIPTION

TCP protocol is a reliable bytestream protocol for transporting data
over network. It is defined in RFC 793.

This function accepts an incoming TCP connection.

**s**: Socket created by **tcp_listen**.

**addr**: Out parameter. IP address of the connecting endpoint. Can be **NULL**.

**deadline**: A point in time when the operation should time out, in milliseconds. Use the **now** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

The socket can be cleanly shut down using **tcp_close** function.

This function is not available if libdill is compiled with **--disable-sockets** option.

# RETURN VALUE

In case of success the function returns handle of the new connection. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EBADF**: Invalid handle.
* **ECANCELED**: Current coroutine was canceled.
* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOMEM**: Not enough memory.
* **ENOTSUP**: The handle does not support this operation.
* **ETIMEDOUT**: Deadline was reached.

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
