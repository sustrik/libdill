# unixdetach(3) manual page

## NAME

unixdetach - detaches dsock UNIX domain handle from the underlying file descriptor

## SYNOPSIS

```
#include <dsock.h>
int unixdetach(int s);
```

## DESCRIPTION

TODO

## RETURN VALUE

Underlying file descriptor. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

TODO

## EXAMPLE

TODO

## SEE ALSO

TODO

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

