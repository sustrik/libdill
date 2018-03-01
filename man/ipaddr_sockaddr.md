# NAME

ipaddr_sockaddr - returns sockaddr structure corresponding to the IP address

# SYNOPSIS

```c
#include <libdill.h>

const struct sockaddr* ipaddr_sockaddr(const struct ipaddr* addr);
```

# DESCRIPTION

Returns **sockaddr** structure corresponding to the IP address.This function is typically used in combination with ipaddr_len topass address and its length to POSIX socket APIs.

**addr**: IP address object.

This function is not available if libdill is compiled with **--disable-sockets** option.

# RETURN VALUE

Pointer to **sockaddr** structure correspoding to the address object.

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

**ipaddr_family**(3) **ipaddr_len**(3) **ipaddr_local**(3) **ipaddr_port**(3) **ipaddr_remote**(3) **ipaddr_setport**(3) **ipaddr_str**(3) 

