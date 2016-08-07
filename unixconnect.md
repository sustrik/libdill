# unixconnect(3) manual page

## NAME

unixconnect - connect to remote UNIX domain endpoint

## SYNOPSIS

```
#include <dsock.h>
int unixconnect(const char *addr, int64_t deadline);
```

## DESCRIPTION

Connects to a remote UNIX domain endpoint. `addr` is the filename to connect to.

## RETURN VALUE

Handle of the created socket. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

TODO

## EXAMPLE

```
int s = unixconnect("/tmp/unix.test", -1);
```

## SEE ALSO

* [ipremote(3)](ipremote.html)
* [unixaccept(3)](unixaccept.html)
* [unixlisten(3)](unixlisten.html)
* [unixrecv(3)](unixrecv.html)
* [unixsend(3)](unixsend.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

