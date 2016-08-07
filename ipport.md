# ipport(3) manual page

## NAME

ipport - get port from an IP address

## SYNOPSIS

```
#include <dsock.h>
int ipport(const ipaddr *addr);
```

## DESCRIPTION

Returns port number of the IP address.

## RETURN VALUE

Port number.

## ERRORS

No errors.

## EXAMPLE

```
/* Listen on ephemeral port. */
ipaddr addr;
int rc = iplocal(&addr, NULL, 0, 0);
int s = tcplisten(&addr, 10);
int port = ipport(&addr);
```

## SEE ALSO

* [ipaddrstr(3)](ipaddrstr.html)
* [ipfamily(3)](ipfamily.html)
* [iplen(3)](iplen.html)
* [iplocal(3)](iplocal.html)
* [ipremote(3)](ipremote.html)
* [ipsetport(3)](ipsetport.html)
* [ipsockaddr(3)](ipsockaddr.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

