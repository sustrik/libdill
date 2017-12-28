# NAME

udp_recv - receives a UDP packet

# SYNOPSIS

**#include &lt;libdill.h>**

**ssize_t udp_recv(int** _s_**, struct ipaddr **\*_addr_**, void **\*_buf_**, size_t** _len_**, int64_t** _deadline_**);**

**ssize_t udp_recvl(int** _s_**, struct ipaddr **\*_addr_**, struct iolist **\*_first_**, struct iolist **\*_last_**, int64_t** _deadline_**);**

# DESCRIPTION

UDP is an unreliable message-based protocol. The size of the message is limited. The protocol has no initial or terminal handshake. A single socket can be used to different destinations.

This function receives a single UDP packet. If _addr_ is not NULL, the argument will be filled in by the address of the sender of the packet.

# RETURN VALUE

The function returns size of the message on success. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EBADF**: The socket handle in invalid.
* **ECANCELED**: Current coroutine is in the process of shutting down.
* **EINVAL**: Invalid arguments.
* **EMSGSIZE**: The message was larger than the supplied buffer.
* **ENOMEM**: Not enough memory.
* **ENOTSUP**: The operation is not supported by the socket.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```
char buf[256];
ssize_t sz = udp_recv(s, NULL, buf, sizeof(buf), -1);
```
