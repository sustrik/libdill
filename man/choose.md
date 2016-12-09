# NAME

choose - perform one of multiple channel operations

# SYNOPSIS

```c
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

# DESCRIPTION

Accepts a list of channel operations. Performs one that can be done first. If multiple operations can be done immediately, one that comes earlier in the array is executed.

`op` is operation code, either `CHSEND` or `CHRECV`. `ch` is a channel handle. `val` is a pointer to buffer to send from or receive to. `len` is size of the buffer, in bytes.

If deadline expires before any operation can be performed, the function fails with `ETIMEDOUT` error.

# RETURN VALUE

Returns -1 if an error occured. Index of the clause that caused the function to exit otherwise. Even in the latter case `errno` may be set to indicate a failure.

# ERRORS

If function returns -1 it sets `errno` to one of the following values:

* `ECANCELED`: Current coroutine is being shut down.
* `ETIMEDOUT`: The deadline was exceeded.

If function returns index of operation it sets `errno` to one of the following values:

* `0`: Operation completed successfully.
* `EBADF`: Invalid handle.
* `EINVAL`: Invalid parameter.
* `ENOTSUP`: Operation not supported. Presumably, the handle isn't a channel.
* `EPIPE`: The channel was closed using `chdone` function.

# EXAMPLE

```c
int val1 = 0;
int val2;
struct chclause clauses[] = {
    {CHSEND, ch, &val1, sizeof(val1)},
    {CHRECV, ch, &val2, sizeof(val2)}
};
int result = choose(clauses, 2, now() + 1000);
if(result == -1) {
    perror("Choose failed");
    exit(1);
}
if(result == 0) {
    printf("Value %d sent.\n", val1);
}
if(result == 1) {
    printf("Value %d received.\n", val2);
}
```

