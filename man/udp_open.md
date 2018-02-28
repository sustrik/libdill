# NAME

udp_open - opens an UDP socket

# SYNOPSIS

```c
#include <libdill.h>

int udp_open(struct ipaddr* local, struct ipaddr* remote);
```

# DESCRIPTION

UDP is an unreliable message-based protocol defined in RFC 768. The size
of the message is limited. The protocol has no initial or terminal
handshake. A single socket can be used to different destinations.

This function creates an UDP socket.

**local**: IP  address to be used to set source IP address in outgoing packets. Also, the socket will receive packets sent to this address. If port in the address is set to zero an ephemeral port will be chosen and filled into the local address.

**remote**: IP address used as default destination for outbound packets. It is used when destination address in **udp_send** function is set to **NULL**. It is also used by **msend** and **mrecv** functions which don't allow to specify the destination address explicitly.

To close this socket use **hclose** function.

This function is not available if libdill is compiled with **--disable-sockets** option.

# RETURN VALUE

In case of success the function returns newly created socket handle. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EADDRINUSE**: The local address is already in use.
* **EADDRNOTAVAIL**: The specified address is not available from the local machine.
* **EINVAL**: Invalid argument.
* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOMEM**: Not enough memory.

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

hclose(3) udp_open(3) udp_open_mem(3) udp_recv(3) udp_recvl(3) udp_send(3) udp_sendl(3) 
