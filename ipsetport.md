# ipsetport(3) manual page

## NAME

ipsetport - sets port of an IP address

## SYNOPSIS

```
#include <dsock.h>
void ipsetport(ipaddr *addr, int port);
```

## DESCRIPTION

Sets the port of the IP address `addr` to the value supplied in `port` argument.

## RETURN VALUE

No return value.

## ERRORS

No errors.

## EXAMPLE

```
ipsetport(&addr, 5555);
```

## SEE ALSO

* [ipaddrstr(3)](ipaddrstr.html)
* [ipfamily(3)](ipfamily.html)
* [iplen(3)](iplen.html)
* [iplocal(3)](iplocal.html)
* [ipport(3)](ipport.html)
* [ipremote(3)](ipremote.html)
* [ipsockaddr(3)](ipsockaddr.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

