# NAME

ipaddr_family - returns family of the IP address

# SYNOPSIS

```c
#include <libdill.h>

int ipaddr_family(const struct ipaddr* addr);
```

# DESCRIPTION

Returns family of the address, i.e.  either AF_INET for IPv4
addresses or AF_INET6 for IPv6 addresses.

**addr**: IP address object.

# RETURN VALUE

IP family.

# ERRORS

None.

# EXAMPLE

```c
ipaddr addr;
ipaddr_remote(&addr, "www.example.org", 80, 0, -1);
int s = socket(ipaddr_family(addr), SOCK_STREAM, 0);
connect(s, ipaddr_sockaddr(&addr), ipaddr_len(&addr));
```
