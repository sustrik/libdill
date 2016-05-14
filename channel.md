# channel(3) manual page

## NAME

channel() - create a channel

## SYNOPSIS

```
#include <libdill.h>
int channel(size_t itemsz, size_t bufsz);
```

## DESCRIPTION

## RETURN VALUE

Returns a channel handle. In the case of error it returns -1 and sets `errno` to one of the values below.

## ERRORS

* `ECANCELED`: Current coroutine is in the process of shutting down.
* `ENOMEM`: Not enough memory to allocate the channel.

## EXAMPLE

## SEE ALSO

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

