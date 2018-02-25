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
int family = ipaddr_family(&addr);
```
