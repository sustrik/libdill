# now(3) manual page

## NAME

now - get current time

## SYNOPSIS

```
#include <libdill.h>
int64_t now(void);
```

## DESCRIPTION

Returns current time, in milliseconds.

The function is meant to be used for creating deadlines. For example, a point of time one second on from now can be expressed as `now() + 1000`.

Following values have special meaning and cannot be returned by the function:

* *0*: Immediate.
* *-1*: Infinite.

## RETURN VALUE

Current time.

## ERRORS

None.

## EXAMPLE

```
chrecv(ch, &val, sizeof(val), now() + 1000);
```

## SEE ALSO

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

