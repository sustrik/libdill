# NAME

ipaddr_local - resolve the address of a local network interface

# SYNOPSIS

```c
#include <libdill.h>

int ipaddr_local(struct ipaddr* addr, const char* name, int port, int mode);
```

# DESCRIPTION

Converts an IP address in human-readable format, or a name of a
local network interface into an **ipaddr** structure.

Mode specifies which kind of addresses should be returned. Possible
values are:

* **IPADDR_IPV4**: Get IPv4 address.
* **IPADDR_IPV6**: Get IPv6 address.
* **IPADDR_PREF_IPV4**: Get IPv4 address if possible, IPv6 address otherwise.
* **IPADDR_PREF_IPV6**: Get IPv6 address if possible, IPv4 address otherwise.

Setting the argument to zero invokes default behaviour, which, at the
present, is **IPADDR_PREF_IPV4**. However, in the future when IPv6 becomes
more common it may be switched to **IPADDR_PREF_IPV6**.

**addr**: Out parameter, The IP address object.

**name**: Name of the local network interface, such as "eth0", "192.168.0.111" or "::1".

**port**: Port number. Valid values are 1-65535.

**mode**: What kind of address to return. See above.

# RETURN VALUE

In case of success the function returns 0. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **ENODEV**: Local network interface with the specified name does not exist.

# EXAMPLE

```c
struct ipaddr addr;
ipaddr_local(&addr, "eth0", 5555, 0);
int s = socket(ipaddr_family(addr), SOCK_STREAM, 0);
bind(s, ipaddr_sockaddr(&addr), ipaddr_len(&addr));
```
# SEE ALSO

ipaddr_family(3) ipaddr_len(3) ipaddr_local(3) ipaddr_port(3) ipaddr_remote(3) ipaddr_setport(3) ipaddr_sockaddr(3) ipaddr_str(3) 
