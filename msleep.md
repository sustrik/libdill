# msleep(3) manual page

## NAME

msleep - waits until deadline

## SYNOPSIS

```
#include <libdill.h>
int msleep(int64_t deadline);
```

## DESCRIPTION

Waits until the deadline expires.

## RETURN VALUE

Returns 0 in case of success, -1 in case of error. In the latter case it sets `errno` to one of the values below.

## ERRORS

* `ECANCELED`: Current coroutine is being shut down.

## EXAMPLE

```
msleep(now() + 1000);
```

## SEE ALSO

[now(3)](now.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

