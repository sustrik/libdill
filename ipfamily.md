# ipfamily(3) manual page

## NAME

ipfamily - returns IP family of the IP address

## SYNOPSIS

```
#include <dsock.h>
int ipfamily(const ipaddr *addr);
```

## DESCRIPTION

Returns IP family of the IP address.

## RETURN VALUE

IP protocol family -- either AF_INET or AF_INET6.

## ERRORS

No errors.

## EXAMPLE

```
int family = ipfamily(&addr);
```

## SEE ALSO

* [ipaddrstr(3)](ipaddrstr.html)
* [iplen(3)](iplen.html)
* [iplocal(3)](iplocal.html)
* [ipport(3)](ipport.html)
* [ipremote(3)](ipremote.html)
* [ipsetport(3)](ipsetport.html)
* [ipsockaddr(3)](ipsockaddr.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

