# ipremote(3) manual page

## NAME

ipremote - get remote IP address

## SYNOPSIS

```
#include <dsock.h>
int ipremote(ipaddr *addr, const char *name, int port, int mode, int64_t deadline);
```

## DESCRIPTION

Converts an IP address in human-readable format, or a name of a remote host into an ipaddr object:

```
ipaddr a1 = ipremote("192.168.0.111", 5555, 0, -1);
ipaddr a2 = ipremote("www.expamle.org", 80, 0, -1); 
```

First argument is the destination `ipaddr` object. `name` is the string to convert. `port` is the port number. `mode` specifies which kind of addresses should be returned. Possible values are:

* `IPADDR_IPV4`: get IPv4 address or error if it's not available
* `IPADDR_IPV6`: get IPv6 address or error if it's not available
* `IPADDR_PREF_IPV4`: get IPv4 address if possible, IPv6 address otherwise; error if none is available
* `IPADDR_PREF_IPV6`: get IPv6 address if possible, IPv4 address otherwise; error if none is available

Setting the argument to zero invokes default behaviour, which, at the present, is `IPADDR_PREF_IPV4`. However, in the future when IPv6 becomes more common it may be switched to `IPADDR_PREF_IPV6`.

Finally, the fourth argument is the deadline. It allows to deal with situations where resolving a remote host name requires a DNS query and the query is taking substantial amount of time to complete.

## RETURN VALUE

Zero in case of success. In case of error -1 is returned and `errno` is set to one of the following values.

## ERRORS

As the functionality is not covered by POSIX and rather implemented by OS-specific means there's no definitive list of possible error codes the function can return. You have to make the error handling as generic as possible.

## EXAMPLE

```
ipaddr addr;
int rc = ipremote(&addr, "www.example.org", 5555, 0, -1);
```

## SEE ALSO

* [ipaddrstr(3)](ipaddrstr.html)
* [ipfamily(3)](ipfamily.html)
* [iplen(3)](iplen.html)
* [iplocal(3)](iplocal.html)
* [ipport(3)](ipport.html)
* [ipsetport(3)](ipsetport.html)
* [ipsockaddr(3)](ipsockaddr.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

