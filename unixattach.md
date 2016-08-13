# unixattach(3) manual page

## NAME

unixattach - creates a dsock UNIX domain handle from a raw file descriptor

## SYNOPSIS

```
#include <dsock.h>
int unixattach(int fd);
```

## DESCRIPTION

TODO

## RETURN VALUE

Handle of the created socket. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

TODO

## EXAMPLE

TODO

## SEE ALSO

TODO

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

