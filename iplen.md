# iplen(3) manual page

## NAME

iplen - returns length of the IP address

## SYNOPSIS

```
#include <dsock.h>
int iplen(const ipaddr *addr);
```

## DESCRIPTION

Returns the length of the embedded IP address.

## RETURN VALUE

Size of the address in bytes.

## ERRORS

No errors.

## EXAMPLE

```
ipaddr addr;
rc = ipremote(&addr, "www.example.org", 5555, 0, -1);
rc = connect(s, ipsockaddr(&addr), iplen(&addr));
```

