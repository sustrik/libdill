# fdout(3) manual page

## NAME

fdout - waits while file descriptor becomes writeable

## SYNOPSIS

```
#include <libdill.h>
int fdout(int fd, int64_t deadline);
```

## DESCRIPTION

Waits until file descriptor becomes writeable or until it gets into an error state. Both options cause succeful return from the function. To distinguish between them you have to do subsequent write operation on the file descriptor.

The function also exits if deadline expires.

## RETURN VALUE

The function returns 0 in case of success or -1 in case of error. In the latter case is sets `errno` to one of the following values.

## ERRORS

* `ECANCELED`: Current coroutine is being shut down.
* `EEXIST`: Another coroutine is already doing `fdout` on the file descriptor.
* `ETIMEDOUT`: Deadline expired while waiting for the file descriptor.

## EXAMPLE

```
fdout(fd);
```

## SEE ALSO

[fdclean(3)](fdclean.html)
[fdin(3)](fdin.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

