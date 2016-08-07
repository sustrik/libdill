# unixpair(3) manual page

## NAME

unixpair - create a pair of mutually connected UNIX domain sockets

## SYNOPSIS

```
#include <dsock.h>
int unixpair(
int s[2]);
```

## DESCRIPTION

Creates a pair of mutually connected UNIX domain sockets.

## RETURN VALUE

Zero in case of success. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

TODO

## EXAMPLE

```
int s[2];
int rc = unixpair(s);
```

## SEE ALSO

* [unixaccept(3)](unixaccept.html)
* [unixconnect(3)](unixconnect.html)
* [unixlisten(3)](unixlisten.html)
* [unixrecv(3)](unixrecv.html)
* [unixsend(3)](unixsend.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

