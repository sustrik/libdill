# NAME

udp_recvl - receives an UDP packet

# SYNOPSIS

```c
#include <libdill.h>

ssize_t udp_recvl(int s, struct ipaddr* addr, struct iolist* first, struct iolist* last, int64_t deadline);
```

# DESCRIPTION

UDP is an unreliable message-based protocol defined in RFC 768. The size
of the message is limited. The protocol has no initial or terminal
handshake. A single socket can be used to different destinations.

This function receives a single UDP packet.

This function accepts a linked list of I/O buffers instead of a
single buffer. Argument **first** points to the first item in the
list, **last** points to the last buffer in the list. The list
represents a single, fragmented message, not a list of multiple
messages. Structure **iolist** has the following members:

    void *iol_base;          /* Pointer to the buffer. */
    size_t iol_len;          /* Size of the buffer. */
    struct iolist *iol_next; /* Next buffer in the list. */
    int iol_rsvd;            /* Reserved. Must be set to zero. */

The function returns **EINVAL** error in case the list is malformed
or if it contains loops.

**s**: Handle of the UDP socket.

**addr**: Out parameter. IP address of the sender of the packet. Can be **NULL**.

**first**: Pointer to the first item of a linked list of I/O buffers.

**last**: Pointer to the last item of a linked list of I/O buffers.

**deadline**: A point in time when the operation should time out, in milliseconds. Use the **now** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

This function is not available if libdill is compiled with **--disable-sockets** option.

# RETURN VALUE

In case of success the function returns size of the received message, in bytes. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EBADF**: Invalid handle.
* **ECANCELED**: Current coroutine was canceled.
* **EINVAL**: Invalid argument.
* **EMSGSIZE**: The data won't fit into the supplied buffer.
* **ENOTSUP**: The handle does not support this operation.
* **ETIMEDOUT**: Deadline was reached.

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

now(3) udp_open(3) udp_open_mem(3) udp_recv(3) udp_send(3) udp_sendl(3) 
