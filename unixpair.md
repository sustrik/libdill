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

* `EMFILE`: No more file descriptors are available for this process.
* `ENFILE`: No more file descriptors are available for the system.

## EXAMPLE

```
int s[2];
int rc = unixpair(s);
```

