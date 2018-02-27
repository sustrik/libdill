# NAME

ipaddr_remote - resolve the address of a remote IP endpoint

# SYNOPSIS

```c
#include <libdill.h>

int ipaddr_remote(struct ipaddr* addr, const char* name, int port, int mode, int64_t deadline);
```

# DESCRIPTION

Converts an IP address in human-readable format, or a name of a
remote host into an **ipaddr** structure.

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

**name**: Name of the remote IP endpoint, such as "www.example.org" or "192.168.0.111".

**port**: Port number. Valid values are 1-65535.

**mode**: What kind of address to return. See above.

**deadline**: A point in time when the operation should time out, in milliseconds. Use the **now** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

# RETURN VALUE

In case of success the function returns 0. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EADDRNOTAVAIL**: The name of the remote host cannot be resolved to an address of the specified type.
* **ECANCELED**: Current coroutine was canceled.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
ipaddr addr;
ipaddr_remote(&addr, "www.example.org", 80, 0, -1);
int s = socket(ipaddr_family(addr), SOCK_STREAM, 0);
connect(s, ipaddr_sockaddr(&addr), ipaddr_len(&addr));
```
