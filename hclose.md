# hclose(3) manual page

## NAME

hclose - closes a handle

## SYNOPSIS

```
#include <libdill.h>
int hclose(int h);
```

## DESCRIPTION

Closes a handle.

Once all handles pointing to the same underlying object are closed the object is deallocated.

NOTE: Given that implementations of close for various handle types are not allowed to do blocking calls, this function is non-blocking.

## RETURN VALUE

Returns 0 in case of success. In case of failure it returns -1 and sets `errno` to one of the following values.

## ERRORS

* `EBADF`: Invalid handle.

## EXAMPLE

```
hclose(h);
```

## SEE ALSO

[handle(3)](handle.html)
[hdata(3)](hdata.html)
[hdup(3)](hdup.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

