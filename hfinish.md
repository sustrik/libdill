# hfinish(3) manual page

## NAME

hfinish - soft-cancels a handle

## SYNOPSIS

```
#include <libdill.h>
int hfinished(int h, int64_t deadline);
```

## DESCRIPTION

Closes a handle.

Once all handles pointing to the same underlying object are closed the object starts to shut down. It tries to finish its work, for example, to flush outstanding data to the network. If it's not able to shut down gracefully within the deadline it is closed forcefully.

## RETURN VALUE

Returns 0 in case of success. In case of failure it returns -1 and sets `errno` to one of the following values.

## ERRORS

* `EBADF`: Invalid handle.
* `ETIMEDOUT`: Timeout expired. The underlying object was closed forcefully and may have not finished its work.

## EXAMPLE

```
hclose(h);
```

## SEE ALSO

[handle(3)](handle.html)
[hclose(3)](hclose.html)
[hdata(3)](hdata.html)
[hdup(3)](hdup.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

