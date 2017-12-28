# NAME

udp_send - sends a UDP packet

# SYNOPSIS


**#include &lt;libdill.h>**

**int udp_send(int** _s_**, const struct ipaddr *\**_addr_**, const void **\*_buf_**, size_t** _len)**);**

**int udp_sendl(int** _s_**, const struct ipaddr *\**_addr_**, struct iolist **\*_first_**, struct iolist **\*_last_**);**

# DESCRIPTION

UDP is an unreliable message-based protocol. The size of the message is limited. The protocol has no initial or terminal handshake. A single socket can be used to different destinations.

This function sends a packet to address _addr_. _addr_ can be set to NULL in which case remote address specified in _udp_send_ function will be used.

Given that UDP protocol is unreliable the function has no deadline. If packet cannot be sent it will be silently dropped.

# RETURN VALUE

The function returns 0 on success. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EBADF**: The socket handle in invalid.
* **ECANCELED**: Current coroutine is in the process of shutting down.
* **EINVAL**: Invalid arguments.
* **EMSGSIZE**: Invalid message size.
* **ENOMEM**: Not enough memory.
* **ENOTSUP**: The operation is not supported by the socket.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```
rc = udp_send(s, NULL, "ABC", 3);
```
