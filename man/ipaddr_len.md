# NAME

ipaddr_len - returns length of the address

# SYNOPSIS

```c
#include <libdill.h>

int ipaddr_len(const struct ipaddr* addr);
```

# DESCRIPTION

Returns lenght of the address, in bytes. This function is typically
used in combination with **ipaddr_sockaddr** to pass address and its
length to POSIX socket APIs.

**addr**: IP address object.

# RETURN VALUE

Length of the IP address.

# ERRORS

None.

# EXAMPLE

```c
ipaddr addr;
ipaddr_remote(&addr, "www.example.org", 80, 0, -1);
int s = socket(ipaddr_family(addr), SOCK_STREAM, 0);
connect(s, ipaddr_sockaddr(&addr), ipaddr_len(&addr));
```
# SEE ALSO

ipaddr_family(3) ipaddr_local(3) ipaddr_port(3) ipaddr_remote(3) ipaddr_setport(3) ipaddr_sockaddr(3) ipaddr_str(3) 
