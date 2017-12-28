# NAME

udp_send - sends a UDP packet

# SYNOPSIS


**#include &lt;libdill.h>**

**int udp_send(int** _s_**, const struct ipaddr *\**_addr_**, const void **\*_buf_**, size_t** _len)**);**

**int udp_sendl(int** _s_**, const struct ipaddr *\**_addr_**, struct iolist **\*_first_**, struct iolist **\*_last_**);**

# DESCRIPTION

UDP is an unreliable message-based protocol. The size of the message is limited. The protocol has no initial or terminal handshake. A single socket can be used to different destinations.

TODO

# RETURN VALUE

The function returns 0 on success. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

TODO

# EXAMPLE

```
rc = udp_send(s, NULL, "ABC", 3);
```
