# iplocal(3) manual page

## NAME

iplocal - get local IP address

## SYNOPSIS

```
#include <dsock.h>
int iplocal(ipaddr *addr, const char *name, int port, int mode);
```

## DESCRIPTION

Converts an IP address in human-readable format, or a name of a local network interface, into an ipaddr object:

```
iplocal(&addr, "127.0.0.1", 3333, 0);
iplocal(&addr, "::1", 4444, 0);
iplocal(&addr, "eth0", 5555, 0);
```

First argument is the destination `ipaddr` object. `name` is the string to convert. `port` is the port number. `mode` specifies which kind of addresses should be returned. Possible values are:

* `IPADDR_IPV4`: get IPv4 address or error if it's not available
* `IPADDR_IPV6`: get IPv6 address or error if it's not available
* `IPADDR_PREF_IPV4`: get IPv4 address if possible, IPv6 address otherwise; error if none is available
* `IPADDR_PREF_IPV6`: get IPv6 address if possible, IPv4 address otherwise; error if none is available

Setting the argument to zero invokes default behaviour, which, at the present, is `IPADDR_PREF_IPV4`. However, in the future when IPv6 becomes more common it may be switched to `IPADDR_PREF_IPV6`.

If address parameter is set to `NULL`, `INADDR_ANY` or `in6addr_any` is returned. This value is useful when binding to all local network interfaces.

## RETURN VALUE

Zero in case of success. In case of error -1 is returned and `errno` is set to one of the following values.

## ERRORS

As the functionality is not covered by POSIX and rather implemented by OS-specific means there's no definitive list of possible error codes the function can return. You have to make the error handling as generic as possible.

## EXAMPLE

```
ipaddr addr;
int rc = iplocal(&addr, "eth0", 5555, 0);
```

