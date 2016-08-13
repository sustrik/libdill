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

