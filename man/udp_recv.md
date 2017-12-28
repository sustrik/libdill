# NAME

udp_recv - receives a UDP packet

# SYNOPSIS

**#include &lt;libdill.h>**

**ssize_t udp_recv(int** _s_**, struct ipaddr **\*_addr_**, void **\*_buf_**, size_t** _len_**, int64_t** _deadline_**);**

**ssize_t udp_recvl(int** _s_**, struct ipaddr **\*_addr_**, struct iolist **\*_first_**, struct iolist **\*_last_**, int64_t** _deadline_**);**

# DESCRIPTION

UDP is an unreliable message-based protocol. The size of the message is limited. The protocol has no initial or terminal handshake. A single socket can be used to different destinations.

TODO

# RETURN VALUE

The function returns size of the message on success. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

TODO

# EXAMPLE

```
char buf[256];
ssize_t sz = udp_recv(s, NULL, buf, sizeof(buf), -1);
```
