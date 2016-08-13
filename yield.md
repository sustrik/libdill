# yield(3) manual page

## NAME

yield - yields CPU to other coroutines

## SYNOPSIS

```
#include <libdill.h>
int yield(void);
```

## DESCRIPTION

By calling this function you give other coroutines a chance to run.

You should consider using yield() when doing lengthy computations which don't involve natural coroutine switching points such as `chsend`, `chrecv`, `fdin`, `fdout` or `msleep`.

## RETURN VALUE

The function returns 0 in case of success or -1 in case of error. In the latter case it sets `errno` to one of the following values.

## ERRORS

* `ECANCELED`: Current coroutine is being shut down.

## EXAMPLE

```
yield();
```

