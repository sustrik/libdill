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

