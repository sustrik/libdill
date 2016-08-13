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

