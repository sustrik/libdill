# choose(3) manual page

## NAME

choose - perform one of multiple channel operations

## SYNOPSIS

```
#include <libdill.h>

#define CHSEND 1
#define CHRECV 2

struct chclause {
    int op;
    int ch;
    void *val;
    size_t len;
};

int choose(struct chclause *clauses, int nclauses, int64_t deadline);
```

## DESCRIPTION

Accepts a list of channel operations. Performs one that can be done first. If multiple operations can be done immediately, one to perform is chosen at random.

`op` is operation code, either `CHSEND` or `CHRECV`. `ch` is a channel handle. `val` is a pointer to buffer to send from or receive to. `len` is size of the buffer, in bytes.

If deadline expires before any operation can be performed, the function fails with `ETIMEDOUT` error.

NOTE: `choose` is using reseved fields in `chclause` structure to store its internal information. Therefore, same `chclause` instance cannot be used by two overlapping invocations of `choose`.

## RETURN VALUE

Returns -1 if an error occured. Index of the clause that caused the function to exit otherwise. Even in the latter case `errno` may be set to indicate a failure.

## ERRORS

If function returns -1 it sets `errno` to one of the following values:

* `ECANCELED`: Current coroutine is being shut down.
* `ETIMEDOUT`: The deadline was exceeded.

If function returns index of operation it sets `errno` to one of the following values:

* `0`: Operation completed successfully.
* `EBADF`: Invalid handle.
* `EINVAL`: Invalid parameter.
* `ENOTSUP`: Operation not supported. Presumably, the handle isn't a channel.
* `EPIPE`: The channel was closed using `chdone` function.

## EXAMPLE

```
int val1 = 0;
int val2;
struct chclause clauses[] = {
    {CHSEND, ch, &val1, sizeof(val1)},
    {CHRECV, ch, &val2, sizeof(val2)}
};
int result = choose(clauses, 2, now() + 1000);
```

## SEE ALSO

[channel(3)](channel.html)
[chdup(3)](chdup.html)
[chrecv(3)](chrecv.html)
[chsend(3)](chsend.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

