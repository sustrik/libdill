# NAME

udp_open - opens a UDP socket

# SYNOPSIS

**#include &lt;libdill.h>**

**int udp_open(struct ipaddr **\*_local_**, const struct ipaddr **\*_remote_**);**

# DESCRIPTION

UDP is an unreliable message-based protocol. The size of the message is limited. The protocol has no initial or terminal handshake. A single socket can be used to different destinations.

This function creates a UDP socket.

IP address passed in _local_ argument will be used to set source IP address in outgoing packets. Also, the socket can be used to receive packets sent to this address. If port in the address is set to zero an ephemeral port will be chosen and filled into the _local_ address.

IP address passed in _remote_ is the default destination for outbound packets. It is used by **msend()** and **mrecv()** functions which don't allow for specifying the destination address explicitly. It is also used by **udp_send()** and **udp_sendl()** functions if the address parameter of those functions is set to NULL.

# RETURN VALUE

Newly created socket handle. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EADDRINUSE**: The local address is already in use.
* **EADDRNOTAVAIL**: The specified address is not available from the local machine.
* **ECANCELED**: Current coroutine is being shut down.
* **EINVAL**: Invalid arguments.
* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOMEM**: Not enough memory.

# EXAMPLE

```
struct ipaddr addr;
int rc = ipaddr_local(&addr, NULL, 5555, 0);
int s = udp_open(&addr, NULL);
```
