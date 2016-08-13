# udpattach(3) manual page

## NAME

udpattach - creates a dsock UDP handle from a raw file descriptor

## SYNOPSIS

```
#include <dsock.h>
int udpattach(int fd);
```

## DESCRIPTION

Encapsulates raw file descriptor `fd` in dsock TCP socket object. The descriptor is not duplicated, i.e. the caller should not attempt to use it or close it after this call.

## RETURN VALUE

Handle of the created socket. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

* `ECANCELED`: Current coroutine is being shut down.
* `EINVAL`: Invalid argument.
* `ENOMEM`: Not enough free memory to create the handle.

## EXAMPLE

```
int s = udpattach(fd);
```

