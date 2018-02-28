# NAME

udp_send - sends an UDP packet

# SYNOPSIS

```c
#include <libdill.h>

int udp_send(int s, const struct ipaddr* addr, const void* buf, size_t len);
```

# DESCRIPTION

UDP is an unreliable message-based protocol defined in RFC 768. The size
of the message is limited. The protocol has no initial or terminal
handshake. A single socket can be used to different destinations.

This function sends an UDP packet.

Given that UDP protocol is unreliable the function has no deadline.
If packet cannot be sent it will be silently dropped.

**s**: Handle of the UDP socket.

**addr**: IP address to send the packet to. If set to **NULL** remote address specified in **udp_open** function will be used.

**buf**: Data to send.

**len**: Number of bytes to send.

This function is not available if libdill is compiled with **--disable-sockets** option.

# RETURN VALUE

In case of success the function returns 0. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EBADF**: Invalid handle.
* **EINVAL**: Invalid argument.
* **EMSGSIZE**: The message is too long to fit into an UDP packet.
* **ENOTSUP**: The handle does not support this operation.

# EXAMPLE

```c
struct ipaddr local;
ipaddr_local(&local, NULL, 5555, 0);
struct ipaddr remote;
ipaddr_remote(&remote, "server.example.org", 5555, 0, -1);
int s = udp_open(&local, &remote);
udp_send(s1, NULL, "ABC", 3);
char buf[2000];
ssize_t sz = udp_recv(s, NULL, buf, sizeof(buf), -1);
hclose(s);
```
# SEE ALSO

udp_open(3) udp_open_mem(3) udp_recv(3) udp_recvl(3) udp_send(3) udp_sendl(3) 
