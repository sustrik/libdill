# ipsockaddr(3) manual page

## NAME

ipsockaddr - return sockaddr structure of the IP address

## SYNOPSIS

```
#include <dsock.h>
const struct sockaddr *ipsockaddr(const ipaddr *addr);
```

## DESCRIPTION

Returns pointer to the embedded `sockaddr` structure.

## RETURN VALUE

Pointer to the `sockaddr` structure inside the `ipaddr` object. Once the
object is deallocated the pointer becomes invalid.

## ERRORS

No errors.

## EXAMPLE

```
ipaddr addr;
rc = ipremote(&addr, "www.example.org", 5555, 0, -1);
rc = connect(s, ipsockaddr(&addr), iplen(&addr));
```

## SEE ALSO

* [ipaddrstr(3)](ipaddrstr.html)
* [ipfamily(3)](ipfamily.html)
* [iplen(3)](iplen.html)
* [iplocal(3)](iplocal.html)
* [ipport(3)](ipport.html)
* [ipremote(3)](ipremote.html)
* [ipsetport(3)](ipsetport.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

