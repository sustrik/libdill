# ipaddrstr(3) manual page

## NAME

ipaddrstr - formats ipaddr as a human-readable string

## SYNOPSIS

```
#include <dsock.h>
const char *ipaddrstr(const ipaddr *addr, char *ipstr);
```

## DESCRIPTION

Formats ipaddr as a human-readable string.

First argument is the IP address to format, second is the buffer to store the result it. The buffer must be at least `IPADDR_MAXSTRLEN` bytes long.

## RETURN VALUE

The function returns pointer to the formatted string.

## ERRORS

No errors.

## EXAMPLE

```
char buf[IPADDR_MAXSTRLEN];
ipaddrstr(&addr, buf);
printf("address=%s\n", buf);
```

## SEE ALSO

* [ipfamily(3)](ipfamily.html)
* [iplen(3)](iplen.html)
* [iplocal(3)](iplocal.html)
* [ipport(3)](ipport.html)
* [ipremote(3)](ipremote.html)
* [ipsetport(3)](ipsetport.html)
* [ipsockaddr(3)](ipsockaddr.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

