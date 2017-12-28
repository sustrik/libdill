# NAME

udp_open - opens a UDP socket

# SYNOPSIS

**#include &lt;libdill.h>**

**int udp_open(struct ipaddr **\*_local_**, const struct ipaddr **\*_remote_**);**

# DESCRIPTION

UDP is an unreliable message-based protocol. The size of the message is limited. The protocol has no initial or terminal handshake. A single socket can be used to different destinations.

TODO

# RETURN VALUE

Newly created socket handle. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

TODO

# EXAMPLE

```
struct ipaddr addr;
int rc = ipaddr_local(&addr, NULL, 5555, 0);
int s = udp_open(&addr, NULL);
```
