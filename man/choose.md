# NAME

choose - perform one of multiple channel operations

# SYNOPSIS

```c
#define CHSEND 1
#define CHRECV 2

struct chclause {
    int op;
    int ch;
    void *val;
    size_t len;
};
```

**#include &lt;libdill.h>**

**int choose(struct chclause **\*_clauses_**, int** _nclauses_**, int64_t** _deadline_**);**

# DESCRIPTION

Accepts a list of channel operations. Performs one that can be done first. If multiple operations can be done immediately, the one that comes earlier in the array is executed.

_op_ is a code denoting the operation to be done, and it's either **CHSEND** or **CHRECV**. _ch_ is a channel handle. _val_ is a pointer to a buffer to send data from or receive data to. _len_ is the byte size of the buffer.

_deadline_ is a point in time when the operation should time out. Use the **now** function to get the current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

If the deadline expires before any operation can be performed, the function will fail with an **ETIMEDOUT** error.

# RETURN VALUE

Returns -1 if an error occurred. Otherwise, it returns the index of the clause that caused the function to exit. Even in the latter case, _errno_ may be set to indicate failure.

# ERRORS

If the function returns -1, _errno_ is set to one of the following values:

* **ECANCELED**: Current coroutine is being shut down.
* **ETIMEDOUT**: Deadline has expired.

If the function returns an index, it set _errno_ to one of the following values:

* 0: Operation completed successfully.
* **EBADF**: Invalid handle.
* **EINVAL**: Invalid parameter.
* **ENOTSUP**: Operation not supported. Presumably, the handle isn't a channel.
* **EPIPE**: Channel has been closed with **hdone**.

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

