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

