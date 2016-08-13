# unixdetach(3) manual page

## NAME

unixdetach - detaches dsock UNIX domain handle from the underlying file descriptor

## SYNOPSIS

```
#include <dsock.h>
int unixdetach(int s);
```

## DESCRIPTION

Closes dsock object without closing the underlying file descriptor. The descriptor is returned to the user.

Note that the returned file descriptor is set to non-blocking mode.

## RETURN VALUE

Underlying file descriptor. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

* `EBADF`: Invalid handle.
* `ENOTSUP`: Handle is not a UNIX domain socket.

## EXAMPLE

```
int fd = unixdetach(s);
```

