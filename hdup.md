# hdup(3) manual page

## NAME

hdup - duplicates a handle

## SYNOPSIS

```
#include <libdill.h>
int hdup(int h);
```

## DESCRIPTION

Duplicates a handle. Both handles will refer to the same underlying object.

Each duplicate of the handle requires it's own call to `hclose`. The underlying object is deallocated once all the handles pointing to it are closed.

## RETURN VALUE

The duplicated handle in case of success or -1 in case of failure. In the latter case `errno` is set to one of the following values.

## ERRORS

* `EBADF`: Invalid handle.

## EXAMPLE

```
int h2 = hdup(h1);
```

## SEE ALSO

[handle(3)](handle.html)
[hclose(3)](hclose.html)
[hdata(3)](hdata.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

