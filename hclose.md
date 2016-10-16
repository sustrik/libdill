# hclose(3) manual page

## NAME

hclose - hard-cancels a handle

## SYNOPSIS

```c
#include <libdill.h>
int hclose(int h);
```

## DESCRIPTION

Closes a handle.

Once all handles pointing to the same underlying object are closed the object is deallocated immediately.

The function guarantees that all resources will be deallocated, however, it does not guarantee that handle's work will be fully finished. E.g. it may not flush data to network or similar.

## RETURN VALUE

Returns 0 in case of success. In case of failure it returns -1 and sets `errno` to one of the following values.

## ERRORS

* `EBADF`: Invalid handle.

## EXAMPLE

```c
hclose(h);
```

